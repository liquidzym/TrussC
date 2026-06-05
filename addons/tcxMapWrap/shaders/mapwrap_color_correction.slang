// mapwrap_color_correction.slang — Color correction rendering
uniform fs_params {
    bool  enabled;
    float opacity;
    float brightness;
    float contrast;
    float saturation;
    vec3  gamma;
    vec3  lift;
    vec3  gain;
    float black_level;
    float white_level;
};

texture2D tex;
sampler smp;

in vs_in {
    vec2 position;
    vec2 texcoord;
};

in vs_out {
    vec2 uv;
};

vs_out vs_main(vs_in in) {
    vs_out out;
    out.gl_Position = vec4(in.position, 0.0, 1.0);
    out.uv = in.texcoord;
    return out;
}

frag_out fs_main(vs_out in) {
    vec4 color = texture(sampler2D(tex, smp), in.uv);

    if (enabled) {
        // Lift / Gain / Gamma
        color.rgb = color.rgb * gain + lift;
        color.rgb = pow(color.rgb, gamma);

        // Brightness
        color.rgb *= brightness;

        // Contrast
        color.rgb = (color.rgb - 0.5) * contrast + 0.5;

        // Saturation
        float lum = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        color.rgb = mix(vec3(lum), color.rgb, saturation);

        // Black / White level
        color.rgb = mix(vec3(black_level), vec3(white_level), color.rgb);
    }

    color.a *= opacity;
    return color;
}
