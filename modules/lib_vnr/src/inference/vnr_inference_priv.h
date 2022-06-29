#ifndef __VNR_INFERENCE_PRIV_H__
#define __VNR_INFERENCE_PRIV_H__

#include "model/vnr_tensor_arena_size.h"
#include "bfp_math.h"

/** Quantisation spec used to quantise the VNR input features and dequantise the VNR output according to the specification for TensorFlow Lite's 8-bit quantization scheme
 * Quantisation: q = f/input_scale + input_zero_point
 * Dequantisation: f = output_scale * (q + output_zero_point)
 */
typedef struct {
    float_s32_t input_scale_inv; // Inverse of the input scale
    float_s32_t input_zero_point;
    float_s32_t output_scale;
    float_s32_t output_zero_point;
}vnr_model_quant_spec_t;

typedef struct {
    uint64_t tensor_arena[(TENSOR_ARENA_SIZE_BYTES + sizeof(uint64_t) - 1)/sizeof(uint64_t)]; /// Primary memory available to the inference engine
    vnr_model_quant_spec_t quant_spec; /// VNR model quantisation spec
}vnr_ie_state_t;

#ifdef __cplusplus
extern "C" {
#endif
    /**
    * @brief Initialise the quantisation spec structure with constants defined in vnr_quant_spec_defines.h.
    * The vnr_quant_spec_defines.h file is autogenerated and should be regenerated every time the VNR model changes.
    * @param[in] quant_spec Quantisation spec structure
    */
    void vnr_priv_init_quant_spec(vnr_model_quant_spec_t *quant_spec);
    
    /**
     * @brief Quantise VNR features
     * This function quantises the floating point features according to the specification for TensorFlow Lite's 8-bit quantization scheme.
     * @param[out] quantised_patch quantised feature patch
     * @param[in] normalised_patch feature patch before quantisation. Note that this is not passed as a const pointer and the normalised_patch memory is overwritten
     * during quantisation calculation.
     * @param[in] quant_spec TensorFlow Lite's 8-bit quantisation specification
     */
    void vnr_priv_feature_quantise(int8_t *quantised_patch, bfp_s32_t *normalised_patch, const vnr_model_quant_spec_t *quant_spec);

    /**
     * @brief Deuquantise Inference output
     * This function dequantises the VNR model output according to the specification for TensorFlow Lite's 8-bit quantization scheme.
     * @param[out] dequant_output Dequantised output
     * @param[in] quant_output VNR model inference quantised output
     * @param[in] quant_spec TensorFlow Lite's 8-bit quantisation specification
     */
    void vnr_priv_output_dequantise(float_s32_t *dequant_output, const int8_t* quant_output, const vnr_model_quant_spec_t *quant_spec);
#ifdef __cplusplus
}
#endif

#endif