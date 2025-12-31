#extension GL_OES_EGL_image_external : require
precision mediump float;

varying vec2 vTexCoord;

uniform samplerExternalOES uTexture;

void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
