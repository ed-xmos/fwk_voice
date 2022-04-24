// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xcore/hwtimer.h>

#include "vnr_features_api.h"
#include "vnr_inference_api.h"
#include "fileio.h"
#include "wav_utils.h"

static int framenum=0;
static uint32_t max_feature_cycles=0, max_inference_cycles=0;

void test_wav_vnr(const char *in_filename)
{
    file_t input_file, new_slice_file, norm_patch_file, inference_output_file;

    int ret = file_open(&input_file, in_filename, "rb");
    assert((!ret) && "Failed to open file");

    ret = file_open(&new_slice_file, "new_slice.bin", "wb");
    assert((!ret) && "Failed to open file");

    ret = file_open(&norm_patch_file, "normalised_patch.bin", "wb");
    assert((!ret) && "Failed to open file");

    ret = file_open(&inference_output_file, "inference_output.bin", "wb");
    assert((!ret) && "Failed to open file");

    wav_header input_header_struct;
    unsigned input_header_size;
    if(get_wav_header_details(&input_file, &input_header_struct, &input_header_size) != 0){
        printf("error in get_wav_header_details()\n");
        _Exit(1);
    }
    file_seek(&input_file, input_header_size, SEEK_SET);
    // Ensure 16bit wav file
    if(input_header_struct.bit_depth != 16){
        printf("Test works on only 16 bit wav files. %d bit wav input provided\n",input_header_struct.bit_depth);
        assert(0);
    }

    // Ensure input wav file contains correct number of channels 
    if(input_header_struct.num_channels != 1){
        printf("Error: wav num channels(%d) does not match expected 1 channel\n", input_header_struct.num_channels);
        _Exit(1);
    }

    unsigned frame_count = wav_get_num_frames(&input_header_struct);
    // Calculate number of frames in the wav file
    unsigned block_count = frame_count / VNR_FRAME_ADVANCE;
    printf("%d Frames in this test input wav\n",block_count);

    vnr_feature_state_t DWORD_ALIGNED vnr_feature_state;
    vnr_feature_state_init(&vnr_feature_state);

    vnr_feature_state_t DWORD_ALIGNED vnr_feature_state_check;
    vnr_feature_state_init(&vnr_feature_state_check);

    vnr_ie_state_t vnr_ie_state;
    vnr_inference_init(&vnr_ie_state);
    if(vnr_ie_state.input_size != (VNR_PATCH_WIDTH * VNR_MEL_FILTERS)) {
        printf("Error: Feature size mismatch\n");
        assert(0);
    }
    
    uint64_t start_feature_cycles, end_feature_cycles, start_inference_cycles, end_inference_cycles;
    unsigned bytes_per_frame = wav_get_num_bytes_per_frame(&input_header_struct);
    int16_t input_read_buffer[VNR_FRAME_ADVANCE] = {0}; // Array for storing interleaved input read from wav file
    int32_t new_frame[VNR_FRAME_ADVANCE] = {0};
    
    for(unsigned b=0; b<block_count; b++) {
        long input_location =  wav_get_frame_start(&input_header_struct, b * VNR_FRAME_ADVANCE, input_header_size);
        file_seek (&input_file, input_location, SEEK_SET);
        file_read (&input_file, (uint8_t*)&input_read_buffer[0], bytes_per_frame* VNR_FRAME_ADVANCE);
        // Convert to 32 bit
        for(int i=0; i<VNR_FRAME_ADVANCE; i++) {
            new_frame[i] = (int32_t)input_read_buffer[i] << 16; //1.31
        }
        
        // VNR feature extraction
        start_feature_cycles = (uint64_t)get_reference_time();
        bfp_complex_s32_t X;
        vnr_form_input_frame(&vnr_feature_state, &X, new_frame);
        fixed_s32_t *new_slice = vnr_make_slice(&vnr_feature_state, &X);
        vnr_add_new_slice(vnr_feature_state.feature_buffers, new_slice);
        bfp_s32_t normalised_patch;
        vnr_normalise_patch(&vnr_feature_state, &normalised_patch);
        end_feature_cycles = (uint64_t)get_reference_time();

        file_write(&new_slice_file, (uint8_t*)(vnr_feature_state.feature_buffers[VNR_PATCH_WIDTH-1]), VNR_MEL_FILTERS*sizeof(int32_t));       
        file_write(&norm_patch_file, (uint8_t*)&normalised_patch.exp, 1*sizeof(int32_t));
        file_write(&norm_patch_file, (uint8_t*)normalised_patch.data, VNR_PATCH_WIDTH*VNR_MEL_FILTERS*sizeof(int32_t));
        
        // VNR inference
        start_inference_cycles = (uint64_t)get_reference_time();
        float_s32_t inference_output;
        vnr_inference(&vnr_ie_state, &inference_output, &normalised_patch);
        end_inference_cycles = (uint64_t)get_reference_time();
        

        file_write(&inference_output_file, (uint8_t*)&inference_output, sizeof(float_s32_t));

        //profile
        uint32_t fe = (uint32_t)(end_feature_cycles - start_feature_cycles);
        uint32_t ie = (uint32_t)(end_inference_cycles - start_inference_cycles);
        if(max_feature_cycles < fe) {max_feature_cycles = fe;}
        if(max_inference_cycles < ie) {max_inference_cycles = ie;}

        framenum += 1;
        /*if(framenum == 1) {
            break;
        }*/
    }
    printf("Profile: max_feature_extraction_cycles = %ld, max_inference_cycles = %ld, max_total_cycles = %ld\n",max_feature_cycles, max_inference_cycles, max_feature_cycles+max_inference_cycles);
    shutdown_session(); 
}
