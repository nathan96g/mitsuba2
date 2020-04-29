import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

def example_shapegroup():
    from mitsuba.core.xml import load_string

    return load_string("""
        <shape version="2.0.0" type="shapegroup" id = "s1">
            <shape type='sphere'>
                <float name="radius" value="1.0"/>
                <transform name="to_world">
                    <translate x="-2.0"/>
                </transform>
            </shape>
            <shape type='sphere'>
                <float name="radius" value="1.0"/>
            </shape>
            <shape type='sphere'>
                <float name="radius" value="1.0"/>
                <transform name="to_world">
                    <translate x="2.0"/>
                </transform>
            </shape>
        </shape>""".format())

def test01_create(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    s = example_shapegroup()
    assert s is not None
    assert s.primitive_count() == 3
    assert s.effective_primitive_count() == 0
    assert s.surface_area() == 0
    assert not s.bbox().valid()