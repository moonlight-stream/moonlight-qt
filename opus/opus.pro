QT -= core gui

TARGET = opus
TEMPLATE = lib

OPUS_DIR = $$PWD/opus
DEFINES += \
    USE_ALLOCA=1 \
    HAVE_LRINT=1 \
    HAVE_LRINTF=1 \
    OPUS_HAVE_RTCD=1 \
    OPUS_BUILD=1

contains(QT_ARCH, i386) {
    DEFINES += \
        OPUS_X86_MAY_HAVE_SSE=1 \
        OPUS_X86_MAY_HAVE_SSE2=1 \
        OPUS_X86_MAY_HAVE_SSE4_1=1 \
        OPUS_X86_MAY_HAVE_AVX=1
}

# AMD64 implies SSE2 support
contains(QT_ARCH, x86_64) {
    DEFINES += \
        OPUS_X86_MAY_HAVE_SSE=1 \
        OPUS_X86_MAY_HAVE_SSE2=1 \
        OPUS_X86_MAY_HAVE_SSE4_1=1 \
        OPUS_X86_MAY_HAVE_AVX=1 \
        OPUS_X86_PRESUME_SSE=1 \
        OPUS_X86_PRESUME_SSE2=1
}

QMAKE_CFLAGS += -O2

SOURCES_SSE += \
    $$OPUS_DIR/celt/x86/pitch_sse.c

SOURCES_SSE2 += \
    $$OPUS_DIR/celt/x86/pitch_sse2.c \
    $$OPUS_DIR/celt/x86/vq_sse2.c

SOURCES_SSE41 += \
    $$OPUS_DIR/celt/x86/pitch_sse4_1.c \
    $$OPUS_DIR/celt/x86/celt_lpc_sse4_1.c \
    $$OPUS_DIR/silk/x86/NSQ_sse4_1.c \
    $$OPUS_DIR/silk/x86/VAD_sse4_1.c \
    $$OPUS_DIR/silk/x86/NSQ_del_dec_sse4_1.c \
    $$OPUS_DIR/silk/x86/VQ_WMat_EC_sse4_1.c

SOURCES += \
    $$OPUS_DIR/celt/quant_bands.c \
    $$OPUS_DIR/celt/laplace.c \
    $$OPUS_DIR/celt/rate.c \
    $$OPUS_DIR/celt/celt_decoder.c \
    $$OPUS_DIR/celt/celt_encoder.c \
    $$OPUS_DIR/celt/modes.c \
    $$OPUS_DIR/celt/entenc.c \
    $$OPUS_DIR/celt/entcode.c \
    $$OPUS_DIR/celt/cwrs.c \
    $$OPUS_DIR/celt/pitch.c \
    $$OPUS_DIR/celt/entdec.c \
    $$OPUS_DIR/celt/x86/x86_celt_map.c \
    $$OPUS_DIR/celt/x86/x86cpu.c \
    $$OPUS_DIR/celt/mathops.c \
    $$OPUS_DIR/celt/vq.c \
    $$OPUS_DIR/celt/opus_custom_demo.c \
    $$OPUS_DIR/celt/bands.c \
    $$OPUS_DIR/celt/celt.c \
    $$OPUS_DIR/celt/celt_lpc.c \
    $$OPUS_DIR/celt/kiss_fft.c \
    $$OPUS_DIR/celt/mdct.c \
    $$OPUS_DIR/src/opus_decoder.c \
    $$OPUS_DIR/src/opus_compare.c \
    $$OPUS_DIR/src/opus_multistream_encoder.c \
    $$OPUS_DIR/src/opus_demo.c \
    $$OPUS_DIR/src/opus.c \
    $$OPUS_DIR/src/mapping_matrix.c \
    $$OPUS_DIR/src/mlp.c \
    $$OPUS_DIR/src/opus_multistream_decoder.c \
    $$OPUS_DIR/src/repacketizer_demo.c \
    $$OPUS_DIR/src/analysis.c \
    $$OPUS_DIR/src/opus_projection_encoder.c \
    $$OPUS_DIR/src/repacketizer.c \
    $$OPUS_DIR/src/mlp_data.c \
    $$OPUS_DIR/src/opus_projection_decoder.c \
    $$OPUS_DIR/src/opus_multistream.c \
    $$OPUS_DIR/src/opus_encoder.c \
    $$OPUS_DIR/silk/x86/x86_silk_map.c \
    $$OPUS_DIR/silk/NSQ_del_dec.c \
    $$OPUS_DIR/silk/ana_filt_bank_1.c \
    $$OPUS_DIR/silk/stereo_find_predictor.c \
    $$OPUS_DIR/silk/resampler.c \
    $$OPUS_DIR/silk/HP_variable_cutoff.c \
    $$OPUS_DIR/silk/shell_coder.c \
    $$OPUS_DIR/silk/inner_prod_aligned.c \
    $$OPUS_DIR/silk/tables_gain.c \
    $$OPUS_DIR/silk/bwexpander_32.c \
    $$OPUS_DIR/silk/stereo_quant_pred.c \
    $$OPUS_DIR/silk/gain_quant.c \
    $$OPUS_DIR/silk/debug.c \
    $$OPUS_DIR/silk/decoder_set_fs.c \
    $$OPUS_DIR/silk/LPC_inv_pred_gain.c \
    $$OPUS_DIR/silk/tables_pulses_per_block.c \
    $$OPUS_DIR/silk/NLSF_VQ.c \
    $$OPUS_DIR/silk/control_audio_bandwidth.c \
    $$OPUS_DIR/silk/tables_LTP.c \
    $$OPUS_DIR/silk/dec_API.c \
    $$OPUS_DIR/silk/tables_NLSF_CB_WB.c \
    $$OPUS_DIR/silk/bwexpander.c \
    $$OPUS_DIR/silk/tables_NLSF_CB_NB_MB.c \
    $$OPUS_DIR/silk/LP_variable_cutoff.c \
    $$OPUS_DIR/silk/float/LPC_inv_pred_gain_FLP.c \
    $$OPUS_DIR/silk/float/schur_FLP.c \
    $$OPUS_DIR/silk/float/wrappers_FLP.c \
    $$OPUS_DIR/silk/float/autocorrelation_FLP.c \
    $$OPUS_DIR/silk/float/regularize_correlations_FLP.c \
    $$OPUS_DIR/silk/float/warped_autocorrelation_FLP.c \
    $$OPUS_DIR/silk/float/k2a_FLP.c \
    $$OPUS_DIR/silk/float/find_LPC_FLP.c \
    $$OPUS_DIR/silk/float/noise_shape_analysis_FLP.c \
    $$OPUS_DIR/silk/float/scale_vector_FLP.c \
    $$OPUS_DIR/silk/float/apply_sine_window_FLP.c \
    $$OPUS_DIR/silk/float/bwexpander_FLP.c \
    $$OPUS_DIR/silk/float/inner_product_FLP.c \
    $$OPUS_DIR/silk/float/scale_copy_vector_FLP.c \
    $$OPUS_DIR/silk/float/energy_FLP.c \
    $$OPUS_DIR/silk/float/pitch_analysis_core_FLP.c \
    $$OPUS_DIR/silk/float/process_gains_FLP.c \
    $$OPUS_DIR/silk/float/residual_energy_FLP.c \
    $$OPUS_DIR/silk/float/find_pred_coefs_FLP.c \
    $$OPUS_DIR/silk/float/encode_frame_FLP.c \
    $$OPUS_DIR/silk/float/find_LTP_FLP.c \
    $$OPUS_DIR/silk/float/find_pitch_lags_FLP.c \
    $$OPUS_DIR/silk/float/burg_modified_FLP.c \
    $$OPUS_DIR/silk/float/corrMatrix_FLP.c \
    $$OPUS_DIR/silk/float/LTP_scale_ctrl_FLP.c \
    $$OPUS_DIR/silk/float/sort_FLP.c \
    $$OPUS_DIR/silk/float/LPC_analysis_filter_FLP.c \
    $$OPUS_DIR/silk/float/LTP_analysis_filter_FLP.c \
    $$OPUS_DIR/silk/stereo_MS_to_LR.c \
    $$OPUS_DIR/silk/log2lin.c \
    $$OPUS_DIR/silk/resampler_private_up2_HQ.c \
    $$OPUS_DIR/silk/resampler_rom.c \
    $$OPUS_DIR/silk/A2NLSF.c \
    $$OPUS_DIR/silk/NLSF_decode.c \
    $$OPUS_DIR/silk/resampler_down2.c \
    $$OPUS_DIR/silk/NLSF2A.c \
    $$OPUS_DIR/silk/table_LSF_cos.c \
    $$OPUS_DIR/silk/control_codec.c \
    $$OPUS_DIR/silk/pitch_est_tables.c \
    $$OPUS_DIR/silk/NLSF_unpack.c \
    $$OPUS_DIR/silk/resampler_private_down_FIR.c \
    $$OPUS_DIR/silk/decode_frame.c \
    $$OPUS_DIR/silk/PLC.c \
    $$OPUS_DIR/silk/check_control_input.c \
    $$OPUS_DIR/silk/decode_indices.c \
    $$OPUS_DIR/silk/encode_pulses.c \
    $$OPUS_DIR/silk/LPC_fit.c \
    $$OPUS_DIR/silk/enc_API.c \
    $$OPUS_DIR/silk/init_decoder.c \
    $$OPUS_DIR/silk/biquad_alt.c \
    $$OPUS_DIR/silk/resampler_private_IIR_FIR.c \
    $$OPUS_DIR/silk/resampler_private_AR2.c \
    $$OPUS_DIR/silk/interpolate.c \
    $$OPUS_DIR/silk/stereo_encode_pred.c \
    $$OPUS_DIR/silk/sort.c \
    $$OPUS_DIR/silk/process_NLSFs.c \
    $$OPUS_DIR/silk/tables_other.c \
    $$OPUS_DIR/silk/encode_indices.c \
    $$OPUS_DIR/silk/NLSF_encode.c \
    $$OPUS_DIR/silk/VQ_WMat_EC.c \
    $$OPUS_DIR/silk/NSQ.c \
    $$OPUS_DIR/silk/NLSF_stabilize.c \
    $$OPUS_DIR/silk/CNG.c \
    $$OPUS_DIR/silk/sum_sqr_shift.c \
    $$OPUS_DIR/silk/tables_pitch_lag.c \
    $$OPUS_DIR/silk/NLSF_del_dec_quant.c \
    $$OPUS_DIR/silk/init_encoder.c \
    $$OPUS_DIR/silk/decode_core.c \
    $$OPUS_DIR/silk/decode_pulses.c \
    $$OPUS_DIR/silk/stereo_decode_pred.c \
    $$OPUS_DIR/silk/VAD.c \
    $$OPUS_DIR/silk/decode_pitch.c \
    $$OPUS_DIR/silk/NLSF_VQ_weights_laroia.c \
    $$OPUS_DIR/silk/control_SNR.c \
    $$OPUS_DIR/silk/code_signs.c \
    $$OPUS_DIR/silk/quant_LTP_gains.c \
    $$OPUS_DIR/silk/sigm_Q15.c \
    $$OPUS_DIR/silk/lin2log.c \
    $$OPUS_DIR/silk/decode_parameters.c \
    $$OPUS_DIR/silk/resampler_down2_3.c \
    $$OPUS_DIR/silk/stereo_LR_to_MS.c \
    $$OPUS_DIR/silk/LPC_analysis_filter.c

INCLUDEPATH += \
    $$OPUS_DIR            \
    $$OPUS_DIR/include    \
    $$OPUS_DIR/src        \
    $$OPUS_DIR/silk       \
    $$OPUS_DIR/celt       \
    $$OPUS_DIR/silk/fixed \
    $$OPUS_DIR/silk/float

CONFIG += warn_off staticlib

win32-msvc* {
    # No flags required to build with SSE on MSVC
    SOURCES += SOURCES_SSE SOURCSE_SSE2 SOURCES_SSE41
}
else {
    sse.name = sse
    sse.input = SOURCES_SSE
    sse.dependency_type = TYPE_C
    sse.variable_out = OBJECTS
    sse.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    sse2.name = sse2
    sse2.input = SOURCES_SSE2
    sse2.dependency_type = TYPE_C
    sse2.variable_out = OBJECTS
    sse2.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse2.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse2 $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    sse41.name = sse41
    sse41.input = SOURCES_SSE41
    sse41.dependency_type = TYPE_C
    sse41.variable_out = OBJECTS
    sse41.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    sse41.commands = $${QMAKE_CXX} $(CXXFLAGS) -msse4.1 $(INCPATH) -c ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}

    QMAKE_EXTRA_COMPILERS += sse sse2 sse41
}
