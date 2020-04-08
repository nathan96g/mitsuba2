#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Instance final: public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, is_shapegroup, m_id, m_bsdf)
    MTS_IMPORT_TYPES(ShapeKDTree, BSDF)

    using typename Base::ScalarSize;

    Instance(const Properties &props){
    
        m_id = props.id(); 
        m_transform = props.animated_transform("to_world", Transform4f());
      
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
       const ShapeKDTree *kdtree = m_shapegroup->kdtree();
       const ScalarBoundingBox3f &bbox = kdtree->bbox();
       if (!bbox.valid()) // the geometry group is empty
           return bbox;
       // Collect Key frame time
       std::set<Float> times;
       for(size_t i=0; i<m_transform->size(); ++i)
            times.insert(m_transform->operator[](i).time);
                 
        if (times.size() == 0) times.insert((Float) 0);

       ScalarBoundingBox3f result;
       for (typename std::set<Float>::iterator it = times.begin(); it != times.end(); ++it) {
           const Transform4f &trafo = m_transform->eval(*it);
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

        const ShapeKDTree *kdtree = m_shapegroup->kdtree();
        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        return kdtree->template ray_intersect<false>(trafo_ray, cache, active);
    }

    Mask ray_test(const Ray3f &ray, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        const ShapeKDTree *kdtree = m_shapegroup->kdtree();
        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        return kdtree->template ray_intersect<true>(trafo_ray, (Float* ) nullptr, active).first;
    }



    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        const ShapeKDTree *kdtree = m_shapegroup->kdtree();
        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);

        // create_surface_interaction use the cache to fill correctly the surface interaction
        SurfaceInteraction3f si (kdtree->create_surface_interaction(trafo_ray, si_out.t, cache));

        // todo
        //si.wi = normalize(trafo * si.wi);
        si.sh_frame.n = normalize(trafo * si.sh_frame.n);
        //si.sh_frame.s = trafo * si.sh_frame.s;
        //si.sh_frame.t = trafo * si.sh_frame.t;
        si.n = normalize(trafo * si.n);
        si.dp_du = trafo * si.dp_du;
        si.dp_dv = trafo * si.dp_dv;
        si.p = trafo * si.p;
        si.instance = this;
        si_out[active] = si;
    }

    //std::pair<Vector3f, Vector3f> normal_derivative(const SurfaceInteraction3f &si,
    //                                                bool shading_frame,
    //                                                Mask active) const override {
    //    MTS_MASK_ARGUMENT(active);
//
    //    /* Compute the inverse transformation */
    //    const Transform4f &trafo = m_transform->eval(si.time);
    //    const Transform4f inv_trafo = trafo.inverse();
//
    //    /* The following is really super-inefficient, but it's
    //       needed to be able to deal with general transformations */
    //    SurfaceInteraction3f temp(si);
    //    temp.p = inv_trafo * si.p;
    //    temp.dp_du = inv_trafo * si.dp_du;
    //    temp.dp_dv = inv_trafo * si.dp_dv;
//
    //    /* Determine the length of the transformed normal
    //       *before* it was re-normalized */
    //    Normal3f tn = trafo * normalize(inv_trafo * si.sh_frame.n);
    //    ScalarFloat inv_len = 1 / norm(tn);
    //    tn *= inv_len;
//
    //    std::pair<Vector3f, Vector3f> n_d(si.shape->normal_derivative(temp, shading_frame, active));
//
    //    n_d.first = trafo * Normal3f(n_d.first) * inv_len;
    //    n_d.second = trafo * Normal3f(n_d.second) * inv_len;
//
    //    n_d.first -= tn * dot(tn, n_d.first);
    //    n_d.second -= tn * dot(tn, n_d.second);
//
    //    return n_d;
    //}

    //! @}
    // =============================================================

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