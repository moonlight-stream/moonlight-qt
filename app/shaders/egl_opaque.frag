#extension GL_OES_EGL_image_external : require
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 vTexCoord;

uniform samplerExternalOES uTexture;
uniform vec2 videoSize;
uniform bool nearestNeighbor;

void main() {
    vec2 texCoord = vTexCoord;
    if (nearestNeighbor) {
        texCoord = (floor(texCoord * videoSize) + vec2(0.5)) / videoSize;
    }
    gl_FragColor = texture2D(uTexture, texCoord);
}
