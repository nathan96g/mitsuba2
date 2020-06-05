#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/kdtree.h>

#if defined(MTS_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

// description of shapegroup

template <typename Float, typename Spectrum>
class ShapeGroup final: public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, is_emitter, is_sensor, m_id, m_shapegroup)
    MTS_IMPORT_TYPES(ShapeKDTree)

    using typename Base::ScalarSize;

    ShapeGroup(const Properties &props){
        m_id = props.id(); 

        #if not defined(MTS_ENABLE_EMBREE)
        m_kdtree = new ShapeKDTree(props);
        #endif
        
        // Add all the child or throw an error
        for (auto &kv : props.objects()) {

            const Class *c_class = kv.second->class_();

            if (c_class->name() == "Instance") {
                Throw("Nested instancing is not permitted");
            } else if (c_class->derives_from(MTS_CLASS(Base))) {
                Base *shape = static_cast<Base *>(kv.second.get());
                if (shape->is_shapegroup())
                    Throw("Nested instancing is not permitted");
                if (shape->is_emitter())
                    Throw("Instancing of emitters is not supported");
                if (shape->is_sensor())
                    Throw("Instancing of sensors is not supported");
                else {
                    #if defined(MTS_ENABLE_EMBREE)
                    m_shapes.push_back(shape);
                    m_bbox.expand(shape->bbox());
                    #else
                    m_kdtree->add_shape(shape);
                    #endif
                }
            } else {
                Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
            }
        }

        
        #if not defined(MTS_ENABLE_EMBREE)
        if (m_kdtree->primitive_count() < 100*1024)
            m_kdtree->set_log_level(Trace);
        if (!m_kdtree->ready())
            m_kdtree->build();
        m_bbox = m_kdtree->bbox();
        #endif

        m_shapegroup = true;
    }

    #if defined(MTS_ENABLE_EMBREE)
    ScalarSize primitive_count() const override {
        ScalarSize count = 0;
        for (auto shape : m_shapes)
            count += shape->primitive_count();

        return count;
    }

    void init_embree_scene(RTCDevice device) override{
        if(m_scene == nullptr){ // We construct the BVH only once
            m_scene = rtcNewScene(device);
            for (auto shape : m_shapes)
                rtcAttachGeometry(m_scene, shape->embree_geometry(device));

            rtcCommitScene(m_scene);
        }
    }

    void release_embree_scene() override {
        rtcReleaseScene(m_scene);
    }

    RTCGeometry embree_geometry(RTCDevice device) const override {
        RTCGeometry instance = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        // Ensure that the scene of the shapegroup is build
        if(m_scene == nullptr)
            Throw("Embree scene not initialized, call init_embree_scene() first");
        rtcSetGeometryInstancedScene(instance, m_scene);

        return instance;
    }

    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        SurfaceInteraction3f si(si_out);

        // Extract intersected shape from cache
        if constexpr (!is_array_v<Float>) {
            size_t shape_index = cache[2];
            Assert(shape_index < m_shapes.size());
            si.shape = m_shapes[shape_index];
        } else {
            using ShapePtr = replace_scalar_t<Float, const Base *>;
            UInt32 shape_index = cache[2];
            Assert(shape_index < m_shapes.size());
            si.shape = gather<ShapePtr>(m_shapes.data(), shape_index, active);
        }

        Float extracted_cache[2] = {cache[0], cache[1]};
        si.shape->fill_surface_interaction(ray, extracted_cache, si, active);
        masked(si_out, active) = si;
    }
    #else
    
    ScalarSize primitive_count() const override { return m_kdtree->primitive_count();}

    std::pair<Mask, Float> ray_intersect(const Ray3f &ray, Float * cache,
                                         Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_kdtree->template ray_intersect<false>(ray, cache, active);
    }

    Mask ray_test(const Ray3f &ray, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_kdtree->template ray_intersect<true>(ray, (Float* ) nullptr, active).first;
    }

    void fill_surface_interaction(const Ray3f &ray, const Float * cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        masked(si_out, active) = m_kdtree->create_surface_interaction(ray, si_out.t, cache, active);
    }
    #endif

    // We would have prefere if it returned an invalid bbox
    ScalarBoundingBox3f bbox() const override{ return m_bbox;}

    ScalarFloat surface_area() const override { return 0.f;}

    MTS_INLINE ScalarSize effective_primitive_count() const override { return 0; }

    std::string to_string() const override {
        std::ostringstream oss;
            oss << "ShapeGroup[" << std::endl
                << "  name = \"" << m_id << "\"," << std::endl
                << "  prim_count = " << primitive_count() << std::endl
                << "]";
        return oss.str();
    }

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
private:
    ScalarBoundingBox3f m_bbox;
    
    #if defined(MTS_ENABLE_EMBREE)
        RTCScene m_scene = nullptr;
        std::vector<ref<Base>> m_shapes;
    #else
        ref<ShapeKDTree> m_kdtree;
    #endif
};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MTS_EXPORT_PLUGIN(ShapeGroup, "Grouped geometry for instancing")
NAMESPACE_END(mitsuba)