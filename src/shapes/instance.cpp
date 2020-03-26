#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/kdtree.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Instance final: public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, is_shapegroup)
    MTS_IMPORT_TYPES(ShapeKDTree)

    using typename Base::ScalarSize;

    Instance(const Properties &props){

        // Get the given transform in the xml file or a default one (identity)
        m_transform = props.animated_transform("to_world", Transform4f());
      
        for (auto &kv : props.objects()) {
            Base *shape = dynamic_cast<Base *>(kv.second.get());
            if (shape) {
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

    ScalarSize primitive_count() const override { return 0;}

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
        // TODO Optimization possible 
        return m_shapegroup->ray_test(trafo_ray, active);
    }

    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Throw("fill_surface_interaction 7777777777777777777777777777");

        const Transform4f &trafo = m_transform->eval(ray.time);
        Ray3f trafo_ray(trafo.inverse() * ray);
        m_shapegroup->fill_surface_interaction(trafo_ray, cache, si_out, active);

        si_out.sh_frame.n = normalize(trafo * si_out.sh_frame.n);
        si_out.dp_du = trafo * si_out.dp_du;
        si_out.dp_dv = trafo * si_out.dp_dv;
        si_out.p = trafo * si_out.p;
        si_out.instance = this;
    }

    //! @}
    // =============================================================

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
protected:
    /// Important: declare a protected virtual destructor
   // virtual ~Instance();

private:
   ref<Base> m_shapegroup;
   ref<const AnimatedTransform> m_transform;

};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(Instance, Shape)
MTS_EXPORT_PLUGIN(Instance, "Instanced geometry")
NAMESPACE_END(mitsuba)