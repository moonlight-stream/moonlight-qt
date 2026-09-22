#extension GL_OES_EGL_image_external : require
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 vTexCoord;

uniform mat3 yuvmat;
uniform vec3 offset;
uniform vec2 chromaOffset;
uniform vec2 videoSize;
uniform bool nearestNeighbor;
uniform samplerExternalOES plane1;
uniform samplerExternalOES plane2;

void main() {
    vec2 texCoord = vTexCoord;
    if (nearestNeighbor) {
        texCoord = (floor(texCoord * videoSize) + vec2(0.5)) / videoSize;
    }

    vec3 YCbCr = vec3(
        texture2D(plane1, texCoord)[0],
            texture2D(plane2, texCoord + chromaOffset).xy
    );

    YCbCr -= offset;
    gl_FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
