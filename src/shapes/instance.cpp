#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/bsdf.h>

#if defined(MTS_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
    #include <enoki/transform.h>
#endif

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Instance final: public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, is_shapegroup, m_id, m_bsdf)
    MTS_IMPORT_TYPES(BSDF)

    using typename Base::ScalarSize;

    Instance(const Properties &props){
    
        m_id = props.id(); 
        m_transform = props.animated_transform("to_world", ScalarTransform4f());
      
      for (auto &kv : props.objects()) {
          Base *shape = dynamic_cast<Base *>(kv.second.get());
          if (shape && shape->is_shapegroup()) {
              if (m_shapegroup)
                Throw("Only a single shapegroup can be specified per instance.");
              m_shapegroup = shape;
          } else {
                Throw("Only a shapegroup can be specified in an instance.");
          }
      }

      if (!m_shapegroup)
          Throw("A reference to a 'shapegroup' must be specified!");
    }

   ScalarBoundingBox3f bbox() const override{
       const ScalarBoundingBox3f &bbox = m_shapegroup->bbox();
       if (!bbox.valid()) // the geometry group is empty
           return bbox;
       // Collect Key frame time
       std::set<ScalarFloat> times;
       for(size_t i=0; i<m_transform->size(); ++i)
            times.insert((*m_transform)[i].time);
                 
        if (times.size() == 0) times.insert((ScalarFloat) 0);

       ScalarBoundingBox3f result;
       for (typename std::set<ScalarFloat>::iterator it = times.begin(); it != times.end(); ++it) {
           const ScalarTransform4f &trafo = m_transform->eval(*it);
           for (int i=0; i<8; ++i)
               result.expand(trafo * bbox.corner(i));
       }
       return result;
    }

    ScalarSize primitive_count() const override { return 1;}

    ScalarSize effective_primitive_count() const override { 
        return m_shapegroup->primitive_count();
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

   std::pair<Mask, Float> ray_intersect(const Ray3f &ray, Float * cache,
                                         Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        return m_shapegroup->ray_intersect(trafo_ray, cache, active);
    }

    Mask ray_test(const Ray3f &ray, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        return m_shapegroup->ray_test(trafo_ray, active);
    }

    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        SurfaceInteraction3f si(si_out);
        m_shapegroup->fill_surface_interaction(trafo_ray, cache, si, active);

        si.sh_frame.n = normalize(trafo.transform_affine(si.sh_frame.n));
        si.n = normalize(trafo.transform_affine(si.n));
        si.dp_du = trafo.transform_affine(si.dp_du);
        si.dp_dv = trafo.transform_affine(si.dp_dv);
        si.p = trafo.transform_affine(si.p);
        si.instance = this;
        si_out[active] = si;
    }

    std::pair<Vector3f, Vector3f> normal_derivative(const SurfaceInteraction3f &si,
                                                    bool shading_frame,
                                                    Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        // Compute the inverse transformation
        const Transform4f &trafo = m_transform->eval(si.time);
        const Transform4f inv_trafo = trafo.inverse();

        SurfaceInteraction3f temp(si);
        temp.p = inv_trafo * si.p;
        temp.dp_du = inv_trafo * si.dp_du;
        temp.dp_dv = inv_trafo * si.dp_dv;

        // Determine the length of the transformed normal before it was re-normalized
        Normal3f tn = trafo * normalize(inv_trafo * si.sh_frame.n);
        Float inv_len = 1 / norm(tn);
        tn *= inv_len; // normalize
        
        std::pair<Vector3f, Vector3f> n_d = si.shape->normal_derivative(temp, shading_frame, active);

        // apply inverse transpose to  dndu dans dndv
        n_d.first = trafo * Normal3f(n_d.first) * inv_len;
        n_d.second = trafo * Normal3f(n_d.second) * inv_len;

        n_d.first -= tn * dot(tn, n_d.first);
        n_d.second -= tn * dot(tn, n_d.second);

        return n_d;
    }

    //! @}
    // =============================================================

    #if defined(MTS_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) const override {  
        RTCGeometry instance = m_shapegroup->embree_geometry(device);
        rtcSetGeometryTimeStepCount(instance, 1);

        // Set the transformation for the ray intersect, we eval
        // at t=0 for now, we don't support motion blur yet.
        // we have to eval the transform with an explicit float
        // i.e. 0.0f, otherwise with t=0 the coefficient of the
        // transform will be rounded integer 
        Matrix<float, 4> matrix = m_transform->eval(0.0f).matrix;
        float transform[16];
        for( size_t i = 0; i < 16; ++i)
            transform[i] = matrix(i%4, i/4);

        rtcSetGeometryTransform(instance, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, transform);

        rtcCommitGeometry(instance);

        return instance;
    }
    #endif

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
private:
   ref<Base> m_shapegroup;
   ref<const AnimatedTransform> m_transform;

};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(Instance, Shape)
MTS_EXPORT_PLUGIN(Instance, "Instanced geometry")
NAMESPACE_END(mitsuba)