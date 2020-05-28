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
    MTS_IMPORT_BASE(Shape, is_emitter, is_sensor, m_id)
    MTS_IMPORT_TYPES(ShapeKDTree)

    using typename Base::ScalarSize;

    ShapeGroup(const Properties &props){
        m_id = props.id(); 

        #if defined(MTS_ENABLE_EMBREE)
        scene = nullptr;
        #else
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
        #endif
    }

    #if defined(MTS_ENABLE_EMBREE)
    // We would have prefere if it returned an invalid bbox
    ScalarBoundingBox3f bbox() const override{ return m_bbox;}
    
    ScalarSize primitive_count() const override {
        ScalarSize count = 0;
        for (auto shape : m_shapes)
            count += shape->primitive_count();

        return count;
    }

    RTCScene embree_scene(RTCDevice device) {
        if(scene == nullptr){ // We construct the BVH only once
            scene = rtcNewScene(device);
            for (auto shape : m_shapes)
                rtcAttachGeometry(scene, shape->embree_geometry(device));

            rtcCommitScene(scene);
        }
        return scene;
    }

    RTCGeometry embree_geometry(RTCDevice device) const override {
        RTCGeometry instance = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        // get scene from the shapegroup
        RTCScene scene  = (const_cast<ShapeGroup*>(this))->embree_scene(device);
        rtcSetGeometryInstancedScene(instance, scene);

        return instance;
    }

    const Base *shape(size_t i) const override { Assert(i < m_shapes.size()); return m_shapes[i]; }
    #else
    // We would have prefere if it returned an invalid bbox
    ScalarBoundingBox3f bbox() const override{ return m_kdtree->bbox();}
    
    ScalarSize primitive_count() const override { return m_kdtree->primitive_count();}
    
    /// Return a pointer to the internal KD-tree
    const ShapeKDTree *kdtree() const  override { return m_kdtree.get(); }
    #endif

    ScalarFloat surface_area() const override { return 0.f;}

    bool is_shapegroup() const override { return true; }

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

    #if defined(MTS_ENABLE_EMBREE)
        RTCScene scene;
        std::vector<ref<Base>> m_shapes;
        ScalarBoundingBox3f m_bbox;
    #else
        ref<ShapeKDTree> m_kdtree;
    #endif
};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MTS_EXPORT_PLUGIN(ShapeGroup, "Grouped geometry for instancing")
NAMESPACE_END(mitsuba)