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
    MTS_IMPORT_BASE(Shape, is_shapegroup, m_bsdf)
    MTS_IMPORT_TYPES(ShapeKDTree, BSDF)

    using typename Base::ScalarSize;

    Instance(const Properties &props){

        // Get the given transform in the xml file or a default one (identity)
        m_transform = props.animated_transform("to_world", Transform4f());
      
        for (auto &kv : props.objects()) {
            
            Base *shape = dynamic_cast<Base *>(kv.second.get());
            BSDF *bsdf = dynamic_cast<BSDF *>(kv.second.get());

            if (shape && shape->is_shapegroup()) {
                if (m_shapegroup )
                  Throw("Only a single shapegroup can be specified per instance.");
                m_shapegroup = shape;
            } else if(bsdf){
                if (m_bsdf)
                    Throw("Only a single BSDF child object can be specified per shape.");
                m_bsdf = bsdf;
            } else
                  Throw("Only a shapegroup can be specified in an instance.");
        }
        if (!m_shapegroup)
            Throw("A reference to a 'shapegroup' must be specified!");

                    // Create a default diffuse BSDF if needed.
        if (!m_bsdf)
            m_bsdf = PluginManager::instance()->create_object<BSDF>(Properties("diffuse"));
    }

   ScalarBoundingBox3f bbox() const override{
       const ScalarBoundingBox3f &bbox = m_shapegroup->shape()->bbox();
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
    ScalarSize effective_primitive_count() const override { return m_shapegroup->shape()->primitive_count();}

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
        return m_shapegroup->shape()->ray_intersect(trafo_ray, cache, active);
    }

    Mask ray_test(const Ray3f &ray, Mask active) const override {
        
        MTS_MASK_ARGUMENT(active);
        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        return m_shapegroup->shape()->ray_test(trafo_ray, active);
    }

    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
                
        SurfaceInteraction3f si(si_out);

        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        m_shapegroup->shape()->fill_surface_interaction(trafo_ray, cache, si, active);

        si.sh_frame.n = normalize(trafo * si.sh_frame.n);
        si.dp_du = trafo * si.dp_du;
        si.dp_dv = trafo * si.dp_dv;
        si.p = trafo * si.p;
        si.instance = this;

        si_out[active] = si;
    }

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