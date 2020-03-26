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
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    ShapeGroup(const Properties &props) {
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
                else {
                    if(m_shape)
                        Throw("Only a single shape can be specified per instance.");
                    m_shape = shape;
                }
            } else {
                Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
            }
    }
    }
    const Base *shape() const override { return m_shape.get() ;}
    ScalarBoundingBox3f bbox() const override{return BoundingBox3f();}
    ScalarFloat surface_area() const override { return 0.f;}
    bool is_shapegroup() const override { return true; }

    ScalarSize primitive_count() const override { return m_shape->primitive_count();}
    ScalarSize effective_primitive_count() const override { return 0; }

    std::string to_string() const override {
        std::ostringstream oss;
            oss << "ShapeGroup[" << std::endl
                << "  name = \"" << m_id << "\"," << std::endl
                << "  prim_count = " << m_shape->primitive_count() << std::endl
                << "]";
        return oss.str();
    }

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
private:
    ref<Base> m_shape;
};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MTS_EXPORT_PLUGIN(ShapeGroup, "Grouped geometry for instancing")
NAMESPACE_END(mitsuba)