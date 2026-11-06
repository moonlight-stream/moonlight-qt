fxc /T vs_5_0 /O3 /Fo d3d11_vertex.fxc d3d11_vertex.hlsl

fxc /T ps_5_0 /O3 /Fo d3d11_overlay_pixel.fxc d3d11_overlay_pixel.hlsl
fxc /T ps_5_0 /O3 /Fo d3d11_yuv420_pixel.fxc d3d11_yuv420_pixel.hlsl
fxc /T ps_5_0 /O3 /Fo d3d11_ayuv_pixel.fxc d3d11_ayuv_pixel.hlsl
fxc /T ps_5_0 /O3 /Fo d3d11_y410_pixel.fxc d3d11_y410_pixel.hlsl
fxc /T ps_5_0 /O3 /Fo d3d11_yuv420_pixel_bicubic.fxc d3d11_yuv420_pixel_scaled.hlsl
fxc /T ps_5_0 /O3 /DUSE_LANCZOS3=1 /Fo d3d11_yuv420_pixel_lanczos.fxc d3d11_yuv420_pixel_scaled.hlsl