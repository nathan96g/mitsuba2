import mitsuba
import pytest
import enoki as ek

# TEST INSTANCING WITH AN MESH OF SPHERE

def example_scene_mesh(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""
        <scene version='2.0.0'>
            <shape type="ply" version="0.5.0">
                <string name="filename" value="data/sphere.ply"/>
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


def example_scene_mesh_inst(scale = (1,1,1), translate = (0,0,0), rot = (0,0,0)):
    from mitsuba.core.xml import load_string

    return load_string("""
        <scene version='2.0.0'>
            <shape type ="shapegroup" id="s1">
                <shape type="ply">
                    <string name="filename" value="data/sphere.ply"/>
                </shape>
            </shape>
            <shape type ="instance">
                <ref id="s1"/>
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

def test01_ray_intersect(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    from mitsuba.core import Ray3f

    s = example_scene_mesh()
    s_inst = example_scene_mesh_inst()
    # grid size
    n = 21
    inv_n = 1.0 / n

    for x in range(n):
        for y in range(n):
            x_coord = (2 * (x * inv_n) - 1)
            y_coord = (2 * (y * inv_n) - 1)
            ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                        time=0.0, wavelengths=[])
            
            si_found = s.ray_test(ray)
            si_found_inst = s_inst.ray_test(ray)

            assert si_found == si_found_inst

            if si_found:
                ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                            time=0.0, wavelengths=[])
                
                si = s.ray_intersect(ray)
                si_inst = s_inst.ray_intersect(ray)

                assert si.prim_index == si_inst.prim_index
                assert si.instance is None
                assert si_inst.instance is not None
                # we don't compare the shape, because the shape of
                # non-instance version is transform before intersection
                # and the instance version is not
                assert ek.allclose(si.t, si_inst.t, atol=2e-2)
                assert ek.allclose(si.time, si_inst.time, atol=2e-2)
                assert ek.allclose(si.p, si_inst.p, atol=2e-2)
                assert ek.allclose(si.wi, si_inst.wi, atol=2e-2)

                u, v = si.shape.normal_derivative(si)
                u_inst, v_inst = si_inst.instance.normal_derivative(si)
            
                assert ek.allclose(ek.normalize(u), ek.normalize(u_inst), atol=2e-2)
                assert ek.allclose(ek.normalize(v), ek.normalize(v_inst), atol=2e-2)


def test02_ray_intersect_transform(variant_scalar_rgb):
    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    from mitsuba.core import Ray3f

    for r in [1, 3]:
        s = example_scene_mesh(scale=(r, r, r),translate=(0,1,0), rot=(0,30,0))
        s_inst = example_scene_mesh_inst(scale=(r, r, r),translate=(0,1,0), rot=(0,30,0))
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
                si_found_inst = s_inst.ray_test(ray)

                assert si_found == si_found_inst

                if si_found:
                    ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                                time=0.0, wavelengths=[])
                    
                    si = s.ray_intersect(ray)
                    si_inst = s_inst.ray_intersect(ray)

                    assert si.prim_index == si_inst.prim_index
                    assert si.instance is None
                    assert si_inst.instance is not None
                    # we don't compare the shape, because the shape of
                    # non-instance version is transform before intersection
                    # and the instance version is not
                    assert ek.allclose(si.t, si_inst.t, atol=2e-2)
                    assert ek.allclose(si.time, si_inst.time, atol=2e-2)
                    assert ek.allclose(si.p, si_inst.p, atol=2e-2)
                    assert ek.allclose(si.wi, si_inst.wi, atol=2e-2)

                    u, v = si.shape.normal_derivative(si, shading_frame=False)
                    u_inst, v_inst = si_inst.instance.normal_derivative(si, shading_frame=False)

                    assert ek.allclose(u, u_inst)
                    assert ek.allclose(v, v_inst)

                    u, v = si.shape.normal_derivative(si)
                    u_inst, v_inst = si_inst.instance.normal_derivative(si)

                    # less close with transformation because of transform, norm and
                    # interpolation are not done in the same order for instanced and
                    # not instanced version. And norm is not a linear operation
                    assert ek.allclose(ek.normalize(u), ek.normalize(u_inst), atol=6e-1)
                    assert ek.allclose(ek.normalize(v), ek.normalize(v_inst), atol=6e-1)