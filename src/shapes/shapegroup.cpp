#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/kdtree.h>

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
        m_kdtree = new ShapeKDTree(props);
        // Add all the child or throw an error
        for (auto &kv : props.objects()) {

            const Class *c_class = kv.second->class_();

            if (c_class->derives_from(MTS_CLASS(ShapeGroup)) || c_class->name() == "Instance") {
                Throw("Nested instancing is not permitted");
            } else if (c_class->derives_from(MTS_CLASS(Base))) {
                Base *shape = static_cast<Base *>(kv.second.get());
                if (shape->is_emitter())
                    Throw("Instancing of emitters is not supported");
                if (shape->is_sensor())
                    Throw("Instancing of sensors is not supported");
                else
                    m_kdtree->add_shape(shape);
            } else {
                Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
            }
        }

        if (m_kdtree->primitive_count() < 100*1024)
            m_kdtree->set_log_level(Trace);
        if (!m_kdtree->ready())
            m_kdtree->build();
    }

    ScalarBoundingBox3f bbox() const override{return ScalarBoundingBox3f();}
    ScalarFloat surface_area() const override { return 0.f;}

    /// Return a pointer to the internal KD-tree
    const ShapeKDTree *kdtree() const  override { return m_kdtree.get(); }
    bool is_shapegroup() const override { return true; }

    ScalarSize primitive_count() const override { 
        return m_kdtree->primitive_count();
        //ScalarSize result = 0;
        //for (size_t i=0; i< m_kdtree->shape_count(); ++i)
        //    result += m_kdtree->shape(i)->primitive_count();
        //return result;
    }

    MTS_INLINE ScalarSize effective_primitive_count() const override { return 0; }

    std::string to_string() const override {
        std::ostringstream oss;
            oss << "ShapeGroup[" << std::endl
                << "  name = \"" << m_id << "\"," << std::endl
                << "  prim_count = " << m_kdtree->primitive_count() << std::endl
                << "]";
        return oss.str();
    }

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
private:
    ref<ShapeKDTree> m_kdtree;
};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MTS_EXPORT_PLUGIN(ShapeGroup, "Grouped geometry for instancing")
NAMESPACE_END(mitsuba)