import mitsuba
import pytest
import enoki as ek


# ANALYTIC SPHERE

def example_scene(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""<scene version='2.0.0'>
        <shape type='sphere'>
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

    return load_string("""
        <scene version='2.0.0'>
            <shape type="shapegroup" id = "s1">
                <shape type='sphere'>
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
                           rot[0], rot[1], rot[2])) 

def example_sphere_inst(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""
        <shape version='2.0.0' type ="instance">
            <shape type="shapegroup" id = "s1">
                <shape type='sphere'>
                    <float name="radius" value="1.0"/>
                </shape>
            </shape>
            <transform name="to_world">
                <scale x="{}" y="{}" z="{}"/>
                <translate x="{}" y="{}" z="{}"/>
                <rotate x="1" angle="{}"/>
                <rotate y="1" angle="{}"/>
                <rotate z="1" angle="{}"/>
            </transform>
        </shape>""".format(scale[0], scale[1], scale[2], 
                       translate[0], translate[1], translate[2],
                       rot[0], rot[1], rot[2]))

def example_sphere(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""
        <shape version='2.0.0' type='sphere'>
            <float name="radius" value="1.0"/>
            <transform name="to_world">
                <scale x="{}" y="{}" z="{}"/>
                <translate x="{}" y="{}" z="{}"/>
                <rotate x="1" angle="{}"/>
                <rotate y="1" angle="{}"/>
                <rotate z="1" angle="{}"/>
            </transform>
        </shape>""".format(scale[0], scale[1], scale[2], 
                       translate[0], translate[1], translate[2],
                       rot[0], rot[1], rot[2]))


def test01_create(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    s = example_sphere_inst()
    assert s is not None
    assert s.primitive_count() == 1
    assert s.effective_primitive_count() == 1
    
def test02_bbox(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    r = 10
    s = example_sphere_inst(scale=(r,r,r))
    b = s.bbox()
    assert b.valid()
    assert ek.allclose(b.center(), [0, 0, 0])
    assert ek.allclose(b.min,  [-r] * 3)
    assert ek.allclose(b.max,  [r] * 3)
    assert ek.allclose(b.extents(), [2 * r] * 3)

    s = example_sphere_inst(translate=(1,2,3))
    assert ek.allclose(s.bbox().center(), [1, 2, 3])

def test03_bbox(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    s1 = example_sphere(scale = (5,5,5), translate = (10,15,20), rot = (30,45,93))
    s2 = example_sphere_inst(scale = (5,5,5), translate = (10,15,20), rot = (30,45,93))
    b1 = s1.bbox()
    b2 = s2.bbox()
    # By construction:
    assert b2.contains(b1)


def test03_ray_intersect_transform(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    from mitsuba.core import Ray3f

    for r in [1, 3]:
        s = example_scene_inst(scale=(r, r, r),translate=(0,1,0), rot=(0,30,0))
        # grid size
        n = 21
        inv_n = 1.0 / n

        for x in range(n):
            for y in range(n):
                x_coord = r * (2 * (x * inv_n) - 1)
                y_coord = r * (2 * (y * inv_n) - 1)

                ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                            time=0.0, wavelengths=[])
                si_found = s.ray_test(ray)

                assert si_found == (x_coord ** 2 + y_coord ** 2 <= r * r) \
                    or ek.abs(x_coord ** 2 + y_coord ** 2 - r * r) < 1e-8

                if si_found:
                    ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                                time=0.0, wavelengths=[])
                    si = s.ray_intersect(ray)
                    ray_u = Ray3f(ray)
                    ray_v = Ray3f(ray)
                    eps = 1e-4
                    ray_u.o += si.dp_du * eps
                    ray_v.o += si.dp_dv * eps
                    si_u = s.ray_intersect(ray_u)
                    si_v = s.ray_intersect(ray_v)
                    if si_u.is_valid():
                        du = (si_u.uv - si.uv) / eps
                        assert ek.allclose(du, [1, 0], atol=2e-2)
                    if si_v.is_valid():
                        dv = (si_v.uv - si.uv) / eps
                        assert ek.allclose(dv, [0, 1], atol=2e-2)