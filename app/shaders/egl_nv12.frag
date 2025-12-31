#extension GL_OES_EGL_image_external : require
precision mediump float;

varying vec2 vTexCoord;

uniform mat3 yuvmat;
uniform vec3 offset;
uniform vec2 chromaOffset;
uniform samplerExternalOES plane1;
uniform samplerExternalOES plane2;

void main() {
    vec3 YCbCr = vec3(
        texture2D(plane1, vTexCoord)[0],
            texture2D(plane2, vTexCoord + chromaOffset).xy
    );

    YCbCr -= offset;
    gl_FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
