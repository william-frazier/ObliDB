/*
 * Copyright (C) 2011-2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// This sample is confined to the communication between a SGX client platform
// and an ISV Application Server. 



#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
// Needed for definition of remote attestation messages.
#include "remote_attestation_result.h"

#include "isv_enclave_u.h"

// Needed to call untrusted key exchange library APIs, i.e. sgx_ra_proc_msg2.
#include "sgx_ukey_exchange.h"

// Needed to get service provider's information, in your real project, you will
// need to talk to real server.
#include "network_ra.h"

// Needed to create enclave and do ecall.
#include "sgx_urts.h"

// Needed to query extended epid group id.
#include "sgx_uae_service.h"

#include "service_provider.h"
#include "../isv_enclave/definitions.h"//structs, enums, fixed constants

#ifndef SAFE_FREE
#define SAFE_FREE(ptr) {if (NULL != (ptr)) {free(ptr); (ptr) = NULL;}}
#endif

// In addition to generating and sending messages, this application
// can use pre-generated messages to verify the generation of
// messages and the information flow.
#include "sample_messages.h"


#define ENCLAVE_PATH "isv_enclave.signed.so"


//use these to keep track of all the structures and their types (added by me, not part of sample code)
int oblivStructureSizes[NUM_STRUCTURES] = {0};
int oblivStructureTypes[NUM_STRUCTURES] = {0};
uint8_t* oblivStructures[NUM_STRUCTURES] = {0}; //hold pointers to start of each oblivious data structure


uint8_t* msg1_samples[] = { msg1_sample1, msg1_sample2 };
uint8_t* msg2_samples[] = { msg2_sample1, msg2_sample2 };
uint8_t* msg3_samples[MSG3_BODY_SIZE] = { msg3_sample1, msg3_sample2 };
uint8_t* attestation_msg_samples[] =
    { attestation_msg_sample1, attestation_msg_sample2};

// Some utility functions to output some of the data structures passed between
// the ISV app and the remote attestation service provider.
void PRINT_BYTE_ARRAY(
    FILE *file, void *mem, uint32_t len)
{
    if(!mem || !len)
    {
        fprintf(file, "\n( null %d %d)\n", mem, len);
        return;
    }
    uint8_t *array = (uint8_t *)mem;
    fprintf(file, "%u bytes:\n{\n", len);
    uint32_t i = 0;
    for(i = 0; i < len - 1; i++)
    {
        fprintf(file, "0x%x, ", array[i]);
        if(i % 8 == 7) fprintf(file, "\n");
    }
    fprintf(file, "0x%x ", array[i]);
    fprintf(file, "\n}\n");
}


void PRINT_ATTESTATION_SERVICE_RESPONSE(
    FILE *file,
    ra_samp_response_header_t *response)
{
    if(!response)
    {
        fprintf(file, "\t\n( null )\n");
        return;
    }

    fprintf(file, "RESPONSE TYPE:   0x%x\n", response->type);
    fprintf(file, "RESPONSE STATUS: 0x%x 0x%x\n", response->status[0],
            response->status[1]);
    fprintf(file, "RESPONSE BODY SIZE: %u\n", response->size);

    if(response->type == TYPE_RA_MSG2)
    {
        sgx_ra_msg2_t* p_msg2_body = (sgx_ra_msg2_t*)(response->body);

        /*
        fprintf(file, "MSG2 gb - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->g_b), sizeof(p_msg2_body->g_b));

        fprintf(file, "MSG2 spid - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->spid), sizeof(p_msg2_body->spid));

        fprintf(file, "MSG2 quote_type : %hx\n", p_msg2_body->quote_type);

        fprintf(file, "MSG2 kdf_id : %hx\n", p_msg2_body->kdf_id);

        fprintf(file, "MSG2 sign_gb_ga - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->sign_gb_ga),
                         sizeof(p_msg2_body->sign_gb_ga));

        fprintf(file, "MSG2 mac - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->mac), sizeof(p_msg2_body->mac));

        fprintf(file, "MSG2 sig_rl - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->sig_rl),
                         p_msg2_body->sig_rl_size);
        */
    }
    else if(response->type == TYPE_RA_ATT_RESULT)
    {
        sample_ra_att_result_msg_t *p_att_result =
            (sample_ra_att_result_msg_t *)(response->body);
        /*
        fprintf(file, "ATTESTATION RESULT MSG platform_info_blob - ");
        PRINT_BYTE_ARRAY(file, &(p_att_result->platform_info_blob),
                         sizeof(p_att_result->platform_info_blob));

        fprintf(file, "ATTESTATION RESULT MSG mac - ");
        PRINT_BYTE_ARRAY(file, &(p_att_result->mac), sizeof(p_att_result->mac));

        fprintf(file, "ATTESTATION RESULT MSG secret.payload_tag - %u bytes\n",
                p_att_result->secret.payload_size);

        fprintf(file, "ATTESTATION RESULT MSG secret.payload - ");
        PRINT_BYTE_ARRAY(file, p_att_result->secret.payload,
                p_att_result->secret.payload_size);
        */
    }
    else
    {
        fprintf(file, "\nERROR in printing out the response. "
                       "Response of type not supported %d\n", response->type);
    }
}

/*
 * Begin Saba's Code
 * OCALLS GO HERE
 *
 * */

void ocall_print(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
    fflush(stdout);
}

void ocall_read_block(int structureId, int index, int blockSize, void *buffer){ //read in to buffer
	if(blockSize == 0){
		printf("unkown oblivious data type\n");
		return;
	}//printf("heer\n");fflush(stdout);
	//printf("index: %d, blockSize: %d structureId: %d\n", index, blockSize, structureId);
	//printf("start %d, addr: %d, expGap: %d\n", oblivStructures[structureId], oblivStructures[structureId]+index*blockSize, index*blockSize);fflush(stdout);
	memcpy(buffer, oblivStructures[structureId]+((long)index*blockSize), blockSize);//printf("heer\n");fflush(stdout);
	//printf("beginning of mac(app)? %d\n", ((Encrypted_Linear_Scan_Block*)(oblivStructures[structureId]+(index*encBlockSize)))->macTag[0]);
	//printf("beginning of mac(buf)? %d\n", ((Encrypted_Linear_Scan_Block*)(buffer))->macTag[0]);

}
void ocall_write_block(int structureId, int index, int blockSize, void *buffer){ //write out from buffer
	if(blockSize == 0){
		printf("unkown oblivious data type\n");
		return;
	}
	//printf("data: %d %d %d %d\n", structureId, index, blockSize, ((int*)buffer)[0]);fflush(stdout);
	//printf("data: %d %d %d\n", structureId, index, blockSize);fflush(stdout);

	/*if(structureId == 3 && blockSize > 1) {
		blockSize = 8000000;//temp
		printf("in structure 3");fflush(stdout);
	}*/
	//printf("here! blocksize %d, index %d, structureId %d\n", blockSize, index, structureId);
	memcpy(oblivStructures[structureId]+((long)index*blockSize), buffer, blockSize);
	//printf("here2\n");
	//debug code
	//printf("pointer 1 %p, pointer 2 %p, difference %d\n", oblivStructures[structureId], oblivStructures[structureId]+(index*encBlockSize), (index*encBlockSize));
	//printf("beginning of mac? %d\n", ((Encrypted_Linear_Scan_Block*)(oblivStructures[structureId]+(index*encBlockSize)))->macTag[0]);
}

void ocall_respond( uint8_t* message, size_t message_size, uint8_t* gcm_mac){
	printf("ocall response\n");
}

void ocall_newStructure(int newId, Obliv_Type type, int size){ //this is actual size, the logical size will be smaller for orams
    //printf("app: initializing structure type %d of capacity %d blocks\n", type, size);
    int encBlockSize = getEncBlockSize(type);
    if(type == TYPE_ORAM) encBlockSize = sizeof(Encrypted_Oram_Bucket);
    //printf("Encrypted blocks of this type get %d bytes of storage\n", encBlockSize);
    oblivStructureSizes[newId] = size;
    oblivStructureTypes[newId] = type;
    long val = (long)encBlockSize*size;
    printf("mallocing %ld bytes\n", val);
    oblivStructures[newId] = (uint8_t*)malloc(val);
    if(!oblivStructures[newId]) {
    	printf("failed to allocate space for structure\n");fflush(stdout);
    }
}

//helpers


/*
 * End Saba's code
 * */


// This sample code doesn't have any recovery/retry mechanisms for the remote
// attestation. Since the enclave can be lost due S3 transitions, apps
// susceptible to S3 transitions should have logic to restart attestation in
// these scenarios.
#define _T(x) x
int main(int argc, char* argv[])
{
    int ret = 0;
    ra_samp_request_header_t *p_msg0_full = NULL;
    ra_samp_response_header_t *p_msg0_resp_full = NULL;
    ra_samp_request_header_t *p_msg1_full = NULL;
    ra_samp_response_header_t *p_msg2_full = NULL;
    sgx_ra_msg3_t *p_msg3 = NULL;
    ra_samp_response_header_t* p_att_result_msg_full = NULL;
    sgx_enclave_id_t enclave_id = 0;
    int enclave_lost_retry_time = 1;
    int busy_retry_time = 4;
    sgx_ra_context_t context = INT_MAX;
    sgx_status_t status = SGX_SUCCESS;
    ra_samp_request_header_t* p_msg3_full = NULL;

    int32_t verify_index = -1;
    int32_t verification_samples = sizeof(msg1_samples)/sizeof(msg1_samples[0]);

    FILE* OUTPUT = stdout;

#define VERIFICATION_INDEX_IS_VALID() (verify_index > 0 && \
                                       verify_index <= verification_samples)
#define GET_VERIFICATION_ARRAY_INDEX() (verify_index-1)

    if(argc > 1)
    {

        verify_index = atoi(argv[1]);

        if( VERIFICATION_INDEX_IS_VALID())
        {
            //fprintf(OUTPUT, "\nVerifying precomputed attestation messages "
            //                "using precomputed values# %d\n", verify_index);
        }
        else
        {
            fprintf(OUTPUT, "\nValid invocations are:\n");
            fprintf(OUTPUT, "\n\tisv_app\n");
            fprintf(OUTPUT, "\n\tisv_app <verification index>\n");
            fprintf(OUTPUT, "\nValid indices are [1 - %d]\n",
                    verification_samples);
            fprintf(OUTPUT, "\nUsing a verification index uses precomputed "
                    "messages to assist debugging the remote attestation "
                    "service provider.\n");
            return -1;
        }
    }

    // Preparation for remote attestation by configuring extended epid group id.
    {
        uint32_t extended_epid_group_id = 0;
        ret = sgx_get_extended_epid_group_id(&extended_epid_group_id);
        if (SGX_SUCCESS != ret)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError, call sgx_get_extended_epid_group_id fail [%s].",
                __FUNCTION__);
            return ret;
        }
        fprintf(OUTPUT, "\nCall sgx_get_extended_epid_group_id success.");

        p_msg0_full = (ra_samp_request_header_t*)
            malloc(sizeof(ra_samp_request_header_t)
            +sizeof(uint32_t));
        if (NULL == p_msg0_full)
        {
            ret = -1;
            goto CLEANUP;
        }
        p_msg0_full->type = TYPE_RA_MSG0;
        p_msg0_full->size = sizeof(uint32_t);

        *(uint32_t*)((uint8_t*)p_msg0_full + sizeof(ra_samp_request_header_t)) = extended_epid_group_id;
        {

            //fprintf(OUTPUT, "\nMSG0 body generated -\n");

            //PRINT_BYTE_ARRAY(OUTPUT, p_msg0_full->body, p_msg0_full->size);

        }
        // The ISV application sends msg0 to the SP.
        // The ISV decides whether to support this extended epid group id.
        //fprintf(OUTPUT, "\nSending msg0 to remote attestation service provider.\n");

        ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
            p_msg0_full,
            &p_msg0_resp_full);
        if (ret != 0)
        {
            fprintf(OUTPUT, "\nError, ra_network_send_receive for msg0 failed "
                "[%s].", __FUNCTION__);
            goto CLEANUP;
        }
        //fprintf(OUTPUT, "\nSent MSG0 to remote attestation service.\n");
    }
    // Remote attestation will be initiated the ISV server challenges the ISV
    // app or if the ISV app detects it doesn't have the credentials
    // (shared secret) from a previous attestation required for secure
    // communication with the server.
    {
        // ISV application creates the ISV enclave.
        int launch_token_update = 0;
        sgx_launch_token_t launch_token = {0};
        memset(&launch_token, 0, sizeof(sgx_launch_token_t));
        do
        {
            ret = sgx_create_enclave(_T(ENCLAVE_PATH),
                                     SGX_DEBUG_FLAG,
                                     &launch_token,
                                     &launch_token_update,
                                     &enclave_id, NULL);
            if(SGX_SUCCESS != ret)
            {
                ret = -1;
                fprintf(OUTPUT, "\nError, call sgx_create_enclave fail [%s].",
                        __FUNCTION__);
                goto CLEANUP;
            }
            fprintf(OUTPUT, "\nCall sgx_create_enclave success.");

            ret = enclave_init_ra(enclave_id,
                                  &status,
                                  false,
                                  &context);
        //Ideally, this check would be around the full attestation flow.
        } while (SGX_ERROR_ENCLAVE_LOST == ret && enclave_lost_retry_time--);

        if(SGX_SUCCESS != ret || status)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError, call enclave_init_ra fail [%s].",
                    __FUNCTION__);
            goto CLEANUP;
        }
        fprintf(OUTPUT, "\nCall enclave_init_ra success.");

        // isv application call uke sgx_ra_get_msg1
        p_msg1_full = (ra_samp_request_header_t*)
                      malloc(sizeof(ra_samp_request_header_t)
                             + sizeof(sgx_ra_msg1_t));
        if(NULL == p_msg1_full)
        {
            ret = -1;
            goto CLEANUP;
        }
        p_msg1_full->type = TYPE_RA_MSG1;
        p_msg1_full->size = sizeof(sgx_ra_msg1_t);
        do
        {
            ret = sgx_ra_get_msg1(context, enclave_id, sgx_ra_get_ga,
                                  (sgx_ra_msg1_t*)((uint8_t*)p_msg1_full
                                  + sizeof(ra_samp_request_header_t)));
            sleep(3); // Wait 3s between retries
        } while (SGX_ERROR_BUSY == ret && busy_retry_time--);
        if(SGX_SUCCESS != ret)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError, call sgx_ra_get_msg1 fail [%s].",
                    __FUNCTION__);
            goto CLEANUP;
        }
        else
        {
            //fprintf(OUTPUT, "\nCall sgx_ra_get_msg1 success.\n");

            //fprintf(OUTPUT, "\nMSG1 body generated -\n");

            //PRINT_BYTE_ARRAY(OUTPUT, p_msg1_full->body, p_msg1_full->size);

        }

        if(VERIFICATION_INDEX_IS_VALID())
        {

            memcpy_s(p_msg1_full->body, p_msg1_full->size,
                     msg1_samples[GET_VERIFICATION_ARRAY_INDEX()],
                     p_msg1_full->size);

            fprintf(OUTPUT, "\nInstead of using the recently generated MSG1, "
                            "we will use the following precomputed MSG1 -\n");

            //PRINT_BYTE_ARRAY(OUTPUT, p_msg1_full->body, p_msg1_full->size);
        }


        // The ISV application sends msg1 to the SP to get msg2,
        // msg2 needs to be freed when no longer needed.
        // The ISV decides whether to use linkable or unlinkable signatures.
        //fprintf(OUTPUT, "\nSending msg1 to remote attestation service provider."
          //              "Expecting msg2 back.\n");


        ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
                                      p_msg1_full,
                                      &p_msg2_full);

        if(ret != 0 || !p_msg2_full)
        {
            fprintf(OUTPUT, "\nError, ra_network_send_receive for msg1 failed "
                            "[%s].", __FUNCTION__);
            if(VERIFICATION_INDEX_IS_VALID())
            {
                fprintf(OUTPUT, "\nBecause we are in verification mode we will "
                                "ignore this error.\n");
                fprintf(OUTPUT, "\nInstead, we will pretend we received the "
                                "following MSG2 - \n");

                SAFE_FREE(p_msg2_full);
                ra_samp_response_header_t* precomputed_msg2 =
                    (ra_samp_response_header_t*)msg2_samples[
                        GET_VERIFICATION_ARRAY_INDEX()];
                const size_t msg2_full_size = sizeof(ra_samp_response_header_t)
                                              +  precomputed_msg2->size;
                p_msg2_full =
                    (ra_samp_response_header_t*)malloc(msg2_full_size);
                if(NULL == p_msg2_full)
                {
                    ret = -1;
                    goto CLEANUP;
                }
                memcpy_s(p_msg2_full, msg2_full_size, precomputed_msg2,
                         msg2_full_size);

                //PRINT_BYTE_ARRAY(OUTPUT, p_msg2_full,
                //                 sizeof(ra_samp_response_header_t)
                //                 + p_msg2_full->size);
            }
            else
            {
                goto CLEANUP;
            }
        }
        else
        {
            // Successfully sent msg1 and received a msg2 back.
            // Time now to check msg2.
            if(TYPE_RA_MSG2 != p_msg2_full->type)
            {

                fprintf(OUTPUT, "\nError, didn't get MSG2 in response to MSG1. "
                                "[%s].", __FUNCTION__);

                if(VERIFICATION_INDEX_IS_VALID())
                {
                    fprintf(OUTPUT, "\nBecause we are in verification mode we "
                                    "will ignore this error.");
                }
                else
                {
                    goto CLEANUP;
                }
            }

            //fprintf(OUTPUT, "\nSent MSG1 to remote attestation service "
            //                "provider. Received the following MSG2:\n");
            //PRINT_BYTE_ARRAY(OUTPUT, p_msg2_full,
             //                sizeof(ra_samp_response_header_t)
              //               + p_msg2_full->size);

            //fprintf(OUTPUT, "\nA more descriptive representation of MSG2:\n");
            //PRINT_ATTESTATION_SERVICE_RESPONSE(OUTPUT, p_msg2_full);

            if( VERIFICATION_INDEX_IS_VALID() )
            {
                // The response should match the precomputed MSG2:
                ra_samp_response_header_t* precomputed_msg2 =
                    (ra_samp_response_header_t *)
                    msg2_samples[GET_VERIFICATION_ARRAY_INDEX()];
                if(memcmp( precomputed_msg2, p_msg2_full,
                   sizeof(ra_samp_response_header_t) + p_msg2_full->size))
                {/*
                    fprintf(OUTPUT, "\nVerification ERROR. Our precomputed "
                                    "value for MSG2 does NOT match.\n");
                    fprintf(OUTPUT, "\nPrecomputed value for MSG2:\n");
                    PRINT_BYTE_ARRAY(OUTPUT, precomputed_msg2,
                                     sizeof(ra_samp_response_header_t)
                                     + precomputed_msg2->size);
                    fprintf(OUTPUT, "\nA more descriptive representation "
                                    "of precomputed value for MSG2:\n");
                    PRINT_ATTESTATION_SERVICE_RESPONSE(OUTPUT,
                                                       precomputed_msg2);*/
                }
                else
                {
                    fprintf(OUTPUT, "\nVerification COMPLETE. Remote "
                                    "attestation service provider generated a "
                                    "matching MSG2.\n");
                }
            }

        }

        sgx_ra_msg2_t* p_msg2_body = (sgx_ra_msg2_t*)((uint8_t*)p_msg2_full
                                      + sizeof(ra_samp_response_header_t));


        uint32_t msg3_size = 0;
        if( VERIFICATION_INDEX_IS_VALID())
        {
            // We cannot generate a valid MSG3 using the precomputed messages
            // we have been using. We will use the precomputed msg3 instead.
            msg3_size = MSG3_BODY_SIZE;
            p_msg3 = (sgx_ra_msg3_t*)malloc(msg3_size);
            if(NULL == p_msg3)
            {
                ret = -1;
                goto CLEANUP;
            }
            memcpy_s(p_msg3, msg3_size,
                     msg3_samples[GET_VERIFICATION_ARRAY_INDEX()], msg3_size);
            fprintf(OUTPUT, "\nBecause MSG1 was a precomputed value, the MSG3 "
                            "we use will also be. PRECOMPUTED MSG3 - \n");
        }
        else
        {
            busy_retry_time = 2;
            // The ISV app now calls uKE sgx_ra_proc_msg2,
            // The ISV app is responsible for freeing the returned p_msg3!!
            do
            {
                ret = sgx_ra_proc_msg2(context,
                                   enclave_id,
                                   sgx_ra_proc_msg2_trusted,
                                   sgx_ra_get_msg3_trusted,
                                   p_msg2_body,
                                   p_msg2_full->size,
                                   &p_msg3,
                                   &msg3_size);
            } while (SGX_ERROR_BUSY == ret && busy_retry_time--);
            if(!p_msg3)
            {
                fprintf(OUTPUT, "\nError, call sgx_ra_proc_msg2 fail. "
                                "p_msg3 = 0x%p [%s].", p_msg3, __FUNCTION__);
                ret = -1;
                goto CLEANUP;
            }
            if(SGX_SUCCESS != (sgx_status_t)ret)
            {
                fprintf(OUTPUT, "\nError, call sgx_ra_proc_msg2 fail. "
                                "ret = 0x%08x [%s].", ret, __FUNCTION__);
                ret = -1;
                goto CLEANUP;
            }
            else
            {
                fprintf(OUTPUT, "\nCall sgx_ra_proc_msg2 success.\n");
                fprintf(OUTPUT, "\nMSG3 - \n");
            }
        }

        //PRINT_BYTE_ARRAY(OUTPUT, p_msg3, msg3_size);

        p_msg3_full = (ra_samp_request_header_t*)malloc(
                       sizeof(ra_samp_request_header_t) + msg3_size);
        if(NULL == p_msg3_full)
        {
            ret = -1;
            goto CLEANUP;
        }
        p_msg3_full->type = TYPE_RA_MSG3;
        p_msg3_full->size = msg3_size;
        if(memcpy_s(p_msg3_full->body, msg3_size, p_msg3, msg3_size))
        {
            fprintf(OUTPUT,"\nError: INTERNAL ERROR - memcpy failed in [%s].",
                    __FUNCTION__);
            ret = -1;
            goto CLEANUP;
        }

        // The ISV application sends msg3 to the SP to get the attestation
        // result message, attestation result message needs to be freed when
        // no longer needed. The ISV service provider decides whether to use
        // linkable or unlinkable signatures. The format of the attestation
        // result is up to the service provider. This format is used for
        // demonstration.  Note that the attestation result message makes use
        // of both the MK for the MAC and the SK for the secret. These keys are
        // established from the SIGMA secure channel binding.
        ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
                                      p_msg3_full,
                                      &p_att_result_msg_full);
        if(ret || !p_att_result_msg_full)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError, sending msg3 failed [%s].", __FUNCTION__);
            goto CLEANUP;
        }


        sample_ra_att_result_msg_t * p_att_result_msg_body =
            (sample_ra_att_result_msg_t *)((uint8_t*)p_att_result_msg_full
                                           + sizeof(ra_samp_response_header_t));
        if(TYPE_RA_ATT_RESULT != p_att_result_msg_full->type)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError. Sent MSG3 successfully, but the message "
                            "received was NOT of type att_msg_result. Type = "
                            "%d. [%s].", p_att_result_msg_full->type,
                             __FUNCTION__);
            goto CLEANUP;
        }
        else
        {
            fprintf(OUTPUT, "\nSent MSG3 successfully. Received an attestation "
                            "result message back\n.");
            if( VERIFICATION_INDEX_IS_VALID() )
            {
                if(memcmp(p_att_result_msg_full->body,
                        attestation_msg_samples[GET_VERIFICATION_ARRAY_INDEX()],
                        p_att_result_msg_full->size) )
                {
                    fprintf(OUTPUT, "\nSent MSG3 successfully. Received an "
                                    "attestation result message back that did "
                                    "NOT match the expected value.\n");
                    fprintf(OUTPUT, "\nEXPECTED ATTESTATION RESULT -");
                    PRINT_BYTE_ARRAY(OUTPUT,
                        attestation_msg_samples[GET_VERIFICATION_ARRAY_INDEX()],
                        p_att_result_msg_full->size);
                }
            }
        }

        fprintf(OUTPUT, "\nATTESTATION RESULT RECEIVED - ");
        //PRINT_BYTE_ARRAY(OUTPUT, p_att_result_msg_full->body,
        //                 p_att_result_msg_full->size);


        if( VERIFICATION_INDEX_IS_VALID() )
        {
            fprintf(OUTPUT, "\nBecause we used precomputed values for the "
                            "messages, the attestation result message will "
                            "not pass further verification tests, so we will "
                            "skip them.\n");
            goto CLEANUP;
        }

        // Check the MAC using MK on the attestation result message.
        // The format of the attestation result message is ISV specific.
        // This is a simple form for demonstration. In a real product,
        // the ISV may want to communicate more information.
        ret = verify_att_result_mac(enclave_id,
                &status,
                context,
                (uint8_t*)&p_att_result_msg_body->platform_info_blob,
                sizeof(ias_platform_info_blob_t),
                (uint8_t*)&p_att_result_msg_body->mac,
                sizeof(sgx_mac_t));
        if((SGX_SUCCESS != ret) ||
           (SGX_SUCCESS != status))
        {
            ret = -1;
            fprintf(OUTPUT, "\nError: INTEGRITY FAILED - attestation result "
                            "message MK based cmac failed in [%s].",
                            __FUNCTION__);
            goto CLEANUP;
        }

        bool attestation_passed = true;
        // Check the attestation result for pass or fail.
        // Whether attestation passes or fails is a decision made by the ISV Server.
        // When the ISV server decides to trust the enclave, then it will return success.
        // When the ISV server decided to not trust the enclave, then it will return failure.
        if(0 != p_att_result_msg_full->status[0]
           || 0 != p_att_result_msg_full->status[1])
        {
            fprintf(OUTPUT, "\nError, attestation result message MK based cmac "
                            "failed in [%s].", __FUNCTION__);
            attestation_passed = false;
        }

        // The attestation result message should contain a field for the Platform
        // Info Blob (PIB).  The PIB is returned by attestation server in the attestation report.
        // It is not returned in all cases, but when it is, the ISV app
        // should pass it to the blob analysis API called sgx_report_attestation_status()
        // along with the trust decision from the ISV server.
        // The ISV application will take action based on the update_info.
        // returned in update_info by the API.  
        // This call is stubbed out for the sample.
        // 
        // sgx_update_info_bit_t update_info;
        // ret = sgx_report_attestation_status(
        //     &p_att_result_msg_body->platform_info_blob,
        //     attestation_passed ? 0 : 1, &update_info);

        // Get the shared secret sent by the server using SK (if attestation
        // passed)
        if(attestation_passed)
        {
            ret = put_secret_data(enclave_id,
                                  &status,
                                  context,
                                  p_att_result_msg_body->secret.payload,
                                  p_att_result_msg_body->secret.payload_size,
                                  p_att_result_msg_body->secret.payload_tag);
            if((SGX_SUCCESS != ret)  || (SGX_SUCCESS != status))
            {
                fprintf(OUTPUT, "\nError, attestation result message secret "
                                "using SK based AESGCM failed in [%s]. ret = "
                                "0x%0x. status = 0x%0x", __FUNCTION__, ret,
                                 status);
                goto CLEANUP;
            }
        }
        fprintf(OUTPUT, "\nSecret successfully received from server.");
        fprintf(OUTPUT, "\nRemote attestation success!");
    }

    //TODO: My untrusted application code goes here
    /*
     *
     *
     *Begin Saba's Code
     *
     *
	*/
    {
    	//free stuff used before
    	ra_free_network_response_buffer(p_msg0_resp_full);
    	ra_free_network_response_buffer(p_msg2_full);
    	ra_free_network_response_buffer(p_att_result_msg_full);

    	    // p_msg3 is malloc'd by the untrusted KE library. App needs to free.
    	SAFE_FREE(p_msg3);
    	SAFE_FREE(p_msg3_full);
    	SAFE_FREE(p_msg1_full);
    	SAFE_FREE(p_msg0_full);


    	total_init(enclave_id, &status);
        if(status != SGX_SUCCESS){
        	printf("key initialization failed %d.\n", status);
        	goto CLEANUP;
        }

    	//trigger oram initialization
    	ra_samp_response_header_t *oramInitMsg_resp = NULL;
        ra_samp_request_header_t* p_oramInitMsg = (ra_samp_request_header_t*) malloc(sizeof(ra_samp_request_header_t));
        if (NULL == p_oramInitMsg)
        {
            ret = -1;
            goto CLEANUP;
        }
        p_oramInitMsg->type = TYPE_TREE_ORAM_INIT;
        p_oramInitMsg->size = 0;
        ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
            p_oramInitMsg,
            &oramInitMsg_resp);
        if (ret != 0)
        {
            //fprintf(OUTPUT, "\nError, ra_network_send_receive for oramInitMsg failed "
             //   "[%s].", __FUNCTION__);
            goto CLEANUP;
        }
        //fprintf(OUTPUT, "\nSent oram init to remote attestation service.\n");

        //now use the request in oramInitMsg to initialize oram

        unsigned int *oramCapacity = (unsigned int*)malloc(sizeof(int));
        memcpy(oramCapacity, oramInitMsg_resp->body, sizeof(int));


        //Tests for database functionalities here

        Condition condition1, condition2, condition3;
        char a = 'a', b = 'b', c='c';
        int low = 1, high = 200;
        condition1.numClauses = 2;
        condition1.fieldNums[0] = 3;
        condition1.fieldNums[1] = 3;
        condition1.conditionType[0] = 0;
        condition1.conditionType[1] = 0;
        condition1.values[0] = (uint8_t*)&a;
        condition1.values[1] = (uint8_t*)&b;
        condition1.nextCondition = &condition2;
        condition2.numClauses = 1;
        condition2.fieldNums[0] = 1;
        condition2.conditionType[0] = 1;
        condition2.values[0] = (uint8_t*)&low;
        condition2.nextCondition = &condition3;
        condition3.numClauses = 1;
        condition3.fieldNums[0] = 1;
        condition3.conditionType[0] = -1;
        condition3.values[0] = (uint8_t*)&high;
        condition3.nextCondition = NULL;

        //test to create table and print it
        createTestTable(enclave_id, (int*)&status, "myTestTable", 10);
        printTable(enclave_id, (int*)&status, "myTestTable");

        //test to satisfy conditions on rows
        Schema s;
        getTableSchema(enclave_id, &s, "myTestTable");
        uint8_t* row1 = (uint8_t*)malloc(BLOCK_DATA_SIZE);
        row1[0] = 'a';
        int val = 260;
        memcpy(&row1[1], &val, 4);
        //row1[1] = 260;
        int val2 = 313;
        memcpy(&row1[5], &val2, 4);
        row1[9] = 'b';
        int output = 0;
        rowMatchesCondition(enclave_id, &output, condition1, row1, s);
        printf("row1 matches condition: %d", output);

        //test to insert, update, delete
        /*
        insertRow(enclave_id, (int*)&status, "myTestTable", row1);
        insertRow(enclave_id, (int*)&status, "myTestTable", row1);
        printTable(enclave_id, (int*)&status, "myTestTable");
        updateRows(enclave_id, (int*)&status, "myTestTable", condition2, 2, &row1[5]);
        printTable(enclave_id, (int*)&status, "myTestTable");
        deleteRows(enclave_id, (int*)&status, "myTestTable", condition2);
        printTable(enclave_id, (int*)&status, "myTestTable");
        */

        //test select aggregate without group
//int selectRows(char* tableName, int colChoice, Condition c, int aggregate, int groupCol, int algChoice)
        selectRows(enclave_id, (int*)&status, "myTestTable", 1, condition2, 0, -1, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");


        //test select continuous:
        /*
        selectRows(enclave_id, (int*)&status, "myTestTable", -1, condition2, -1, -1, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");
        */

        /*
        //test select almost all: (depending on how much extra space is left in the table data structure)
        createTestTable(enclave_id, (int*)&status, "myTestTable2", 110);
        insertRow(enclave_id, (int*)&status, "myTestTable2", row1);
        insertRow(enclave_id, (int*)&status, "myTestTable2", row1);
        printTable(enclave_id, (int*)&status, "myTestTable2");
        selectRows(enclave_id, (int*)&status, "myTestTable2", -1, condition2, -1, -1, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");
        */

        //test select small:
        /*
        createTestTable(enclave_id, (int*)&status, "myTestTable2", 50);
        insertRow(enclave_id, (int*)&status, "myTestTable2", row1);
        insertRow(enclave_id, (int*)&status, "myTestTable2", row1);
        printTable(enclave_id, (int*)&status, "myTestTable2");
        selectRows(enclave_id, (int*)&status, "myTestTable2", -1, condition2, -1, -1, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");
	*/

        //test group by
        //int selectRows(char* tableName, int colChoice, Condition c, int aggregate, int groupCol, int algChoice)
        selectRows(enclave_id, (int*)&status, "myTestTable", 1, condition1, 0, 3, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");


        //test select hash
        /*
        createTestTable(enclave_id, (int*)&status, "myTestTable2", 50);
        insertRow(enclave_id, (int*)&status, "myTestTable2", row1);
        selectRows(enclave_id, (int*)&status, "myTestTable2", -1, condition2, -1, -1, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");
		*/

        //test join
        /*
        createTestTable(enclave_id, (int*)&status, "join1", 50);
        createTestTable(enclave_id, (int*)&status, "join2", 50);
        deleteRows(enclave_id, (int*)&status, "join2", condition1);
        printTable(enclave_id, (int*)&status, "join2");
        joinTables(enclave_id, (int*)&status, "join1", "join2", 1, 1);
        printTable(enclave_id, (int*)&status, "JoinReturn");
        printTable(enclave_id, (int*)&status, "join2");
        selectRows(enclave_id, (int*)&status, "JoinReturn", 1, condition3, 0, 3, -1);
        printTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "ReturnTable");
        deleteTable(enclave_id, (int*)&status, "JoinReturn");
        */



        //ret = newStructure(enclave_id, TYPE_TREE_ORAM, (*oramCapacity*2-1)*BUCKET_SIZE); //real size of oram is bigger than logical size
        //JK, ignore all this, I'm going to call the testing ecall. TODO: clean this and the service_provider up later
        //ret = newStructure(enclave_id, TYPE_LINEAR_SCAN, 7);//as per the requirements of the hard-coded test
        /*run_tests(enclave_id, &status);
        if(ret || status != SGX_SUCCESS){
        	printf("operation failed.\n");
        	goto CLEANUP;
        }*/


        //testMemory(enclave_id, &status);

        //begin performance tests for data structures

        //int numQueries = 50;
        //int numBlocks = pow(2, NUM_BLOCKS_POW)-1;
        //for(int n = 5; n < 6; n++){//make this something like 150 later and run for many different data block sizes
        	//numBlocks = pow(2, n)-1;
            //set up encrypted linear scan

        /*switch(TEST_TYPE){
        case 1:
        {
            printf("setting up encrypted linear scan\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_LINEAR_SCAN);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up encrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            Linear_Scan_Block* b1 = (Linear_Scan_Block*)malloc(sizeof(Linear_Scan_Block));
            //run encrypted linear scan
            //printf("running encrypted linear scan\n");
            time_t startEnc = clock();
            for(int i = 0; i < numQueries; i++) {//printf("here");fflush(stdout);
                testLinScanBlockPerformance(enclave_id, &status, 0, numBlocks/2, b1, sizeof(Linear_Scan_Block));
            }
            time_t endEnc = clock();
            double elapsedEnc = (double)(endEnc - startEnc)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("encrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            printf("Linear_Encrypted| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedEnc);
            //free the memory we used
            free(oblivStructures[0]);
            free(b1);
        }
        	break;
        case 2:
        {
            //set up unencrypted linear scan
            printf("setting up unencrypted linear scan\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_LINEAR_UNENCRYPTED);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up unencrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            Linear_Scan_Block* b2 = (Linear_Scan_Block*)malloc(sizeof(Linear_Scan_Block));
            //run unencrypted linear scan
            //printf("running unencrypted linear scan\n");
            time_t startUnEnc = clock();
            for(int i = 0; i < numQueries; i++) {
                testLinScanBlockUnencryptedPerformance(enclave_id, &status, 0, numBlocks/2, b2, sizeof(Linear_Scan_Block));
            }
            time_t endUnEnc = clock();
            double elapsedUnEnc = (double)(endUnEnc - startUnEnc)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("unencrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            printf("Linear_Unencrypted| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedUnEnc);
            //free the memory we used
            free(oblivStructures[0]);
            free(b2);
        }
        	break;
        case 3:
        {
            //set up oram
            //printf("setting up oram\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_ORAM);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up oram failed.\n");
            	goto CLEANUP;
            }
            Oram_Block* b3 = (Oram_Block*)malloc(sizeof(Oram_Block));
            //initialize all the blocks so we get a realistic test
            for(int i = 0; i < numBlocks; i++){
            	testOramPerformance(enclave_id, &status, 0, i, b3, sizeof(Oram_Block));
            }
            //run oram
            //printf("running oram\n");
            time_t startOram = clock();
            for(int i = 0; i < numQueries; i++) {
                testOramPerformance(enclave_id, &status, 0, numBlocks/2, b3, sizeof(Oram_Block));
            }
            time_t endOram = clock();
            double elapsedOram = (double)(endOram - startOram)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("oram failed.\n");
            	goto CLEANUP;
            }
            printf("ORAM| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedOram);
            //free the memory we used
            //function to free the memory used for the position map
            free_oram(enclave_id, &status, 0);
            free(oblivStructures[0]);
            free(b3);
        }
        	break;
        case 4:
        {
            //set up "safe" oram
            printf("setting up safe oram\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_ORAM);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up oram failed.\n");
            	goto CLEANUP;
            }
            Oram_Block* b3 = (Oram_Block*)malloc(sizeof(Oram_Block));
            //initialize all the blocks so we get a realistic test
            for(int i = 0; i < numBlocks; i++){
            	testOramPerformance(enclave_id, &status, 0, i, b3, sizeof(Oram_Block));
            }
            //run oram
            //printf("running oram\n");

            //temp for distribuction experiment
            oramDistribution(enclave_id, &status, 0);

            break;
            //end temp
            time_t startOram = clock();
            for(int i = 0; i < numQueries; i++) {
                testOramSafePerformance(enclave_id, &status, 0, numBlocks/2, b3, sizeof(Oram_Block));
            }
            time_t endOram = clock();
            double elapsedOram = (double)(endOram - startOram)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("oram failed.\n");
            	goto CLEANUP;
            }
            printf("ORAMSafe| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedOram);
            //free the memory we used
            //function to free the memory used for the position map
            free_oram(enclave_id, &status, 0);
            free(oblivStructures[0]);
            free(b3);
        }
        	break;
        case 5:
        {
            printf("setting up encrypted linear scan write\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_LINEAR_SCAN);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up encrypted linear scan write failed.\n");
            	goto CLEANUP;
            }
            Linear_Scan_Block* b1 = (Linear_Scan_Block*)malloc(sizeof(Linear_Scan_Block));
            //run encrypted linear scan
            //printf("running encrypted linear scan\n");
            time_t startEnc = clock();
            for(int i = 0; i < numQueries; i++) {//printf("here");fflush(stdout);
                testLinScanBlockWritePerformance(enclave_id, &status, 0, numBlocks/2, b1, sizeof(Linear_Scan_Block));
            }
            time_t endEnc = clock();
            double elapsedEnc = (double)(endEnc - startEnc)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("encrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            printf("Linear_EncryptedWrite| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedEnc);
            //free the memory we used
            free(oblivStructures[0]);
            free(b1);
        	break;
        }
        case 6:
        {
            //set up unencrypted linear scan
            printf("setting up unencrypted linear scan write\n");
            setupPerformanceTest(enclave_id, &status, 0, numBlocks, TYPE_LINEAR_UNENCRYPTED);
            if(ret || status != SGX_SUCCESS){
            	printf("setting up unencrypted linear scan write failed.\n");
            	goto CLEANUP;
            }
            Linear_Scan_Block* b2 = (Linear_Scan_Block*)malloc(sizeof(Linear_Scan_Block));
            //run unencrypted linear scan
            //printf("running unencrypted linear scan\n");
            time_t startUnEnc = clock();
            for(int i = 0; i < numQueries; i++) {
                testLinScanBlockUnencryptedWritePerformance(enclave_id, &status, 0, numBlocks/2, b2, sizeof(Linear_Scan_Block));
            }
            time_t endUnEnc = clock();
            double elapsedUnEnc = (double)(endUnEnc - startUnEnc)/(CLOCKS_PER_SEC);
            if(ret || status != SGX_SUCCESS){
            	printf("unencrypted linear scan failed.\n");
            	goto CLEANUP;
            }
            printf("Linear_UnencryptedWrite| numBlocks: %d, BLOCK_DATA_SIZE: %d, numQueries: %d, time: %f\n", numBlocks, BLOCK_DATA_SIZE, numQueries, elapsedUnEnc);
            //free the memory we used
            free(oblivStructures[0]);
            free(b2);
        }
        	break;
        default:
        	printf("invalid TEST_TYPE!\n");
        	break;
        }*/







        //}

        /*testOpOram(enclave_id, &status);
        if(ret || status != SGX_SUCCESS){
        	printf("oram correctness test failed.\n");
        	goto CLEANUP;
        }*/

    }
    /*
     *
     *
     *End Saba's Code
     *
     *
	*/

CLEANUP:
    // Clean-up
    // Need to close the RA key state.
    if(INT_MAX != context)
    {
        int ret_save = ret;
        ret = enclave_ra_close(enclave_id, &status, context);
        if(SGX_SUCCESS != ret || status)
        {
            ret = -1;
            fprintf(OUTPUT, "\nError, call enclave_ra_close fail [%s].",
                    __FUNCTION__);
        }
        else
        {
            // enclave_ra_close was successful, let's restore the value that
            // led us to this point in the code.
            ret = ret_save;
        }
        fprintf(OUTPUT, "\nCall enclave_ra_close success.");
    }

    sgx_destroy_enclave(enclave_id);

    //todo(Saba): need to free stuff that I used in the code I added
    /*moved up
     * ra_free_network_response_buffer(p_msg0_resp_full);
    ra_free_network_response_buffer(p_msg2_full);
    ra_free_network_response_buffer(p_att_result_msg_full);

    // p_msg3 is malloc'd by the untrusted KE library. App needs to free.
    SAFE_FREE(p_msg3);
    SAFE_FREE(p_msg3_full);
    SAFE_FREE(p_msg1_full);
    SAFE_FREE(p_msg0_full);*/
    //printf("\nEnter a character before exit ...\n");
    //getchar();
    return ret;
}

