import mitsuba
import pytest
import enoki as ek


def example_scene_not_inst(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""<scene version='2.0.0'>
        <shape version='2.0.0' type='sphere'>
            <float name="radius" value="1.0"/>
            <transform name="to_world">
                <scale x="{}" y="{}" z="{}"/>
                <translate x="{}" y="{}" z="{}"/>
                <rotate x="1" angle="{}"/>
                <rotate y="1" angle="{}"/>
                <rotate z="1" angle="{}"/>
            </transform>
        </shape>
    </scene>""".format(scale[0], scale[1], scale[2], 
                       translate[0], translate[1], translate[2],
                       rot[0], rot[1], rot[2]))

def example_scene_inst(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""<scene version='2.0.0'>
        <shape version='2.0.0' type="shapegroup" id = "s1">
            <shape version='2.0.0' type='sphere'>
                <float name="radius" value="1.0"/>
            </shape>
        </shape>
        <shape type ="instance">
            <ref id="s1" />
            <transform name="to_world">
                <scale x="{}" y="{}" z="{}"/>
                <translate x="{}" y="{}" z="{}"/>
                <rotate x="1" angle="{}"/>
                <rotate y="1" angle="{}"/>
                <rotate z="1" angle="{}"/>
            </transform>
        </shape>
    </scene>""".format(scale[0], scale[1], scale[2], 
                       translate[0], translate[1], translate[2],
                       rot[0], rot[1], rot[2],))

def test01_create(variant_scalar_rgb):
    assert True
    