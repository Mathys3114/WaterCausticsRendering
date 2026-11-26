struct WaveSample {
    float height;
    vec2 heightGradient;
};

WaveSample evaluateWave(vec2 position, float time, float amplitude)
{
    const float wavelength = 5.5;
    const float speed = 0.8;
    const vec2 waveDirection = vec2(0.8, 0.6);
    const float waveNumber = 6.2831853 / wavelength;

    float phase = waveNumber * dot(waveDirection, position) - waveNumber * speed * time;
    float phaseDerivative = amplitude * (cos(phase) - sin(2.0 * phase));

    WaveSample wave;
    wave.height = amplitude * (sin(phase) + 0.5 * cos(2.0 * phase));
    wave.heightGradient = waveNumber * waveDirection * phaseDerivative;
    return wave;
}
