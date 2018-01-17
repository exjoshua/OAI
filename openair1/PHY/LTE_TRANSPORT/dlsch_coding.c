/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file PHY/LTE_TRANSPORT/dlsch_coding.c
* \brief Top-level routines for implementing Turbo-coded (DLSCH) transport channels from 36-212, V8.6 2009-03
* \author R. Knopp
* \date 2011
* \version 0.1
* \company Eurecom
* \email: knopp@eurecom.fr
* \note
* \warning
*/

#include "PHY/defs.h"
#include "PHY/extern.h"
#include "PHY/CODING/defs.h"
#include "PHY/CODING/extern.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/LTE_TRANSPORT/defs.h"
#include "PHY/LTE_TRANSPORT/proto.h"
#include "SCHED/defs.h"
#include "defs.h"
#include "UTIL/LOG/vcd_signal_dumper.h"
#include "UTIL/LOG/log.h"
#include <syscall.h>

//#define DEBUG_DLSCH_CODING
//#define DEBUG_DLSCH_FREE 1
//#define TD_DECODING

/*
#define is_not_pilot(pilots,first_pilot,re) (pilots==0) || \
  ((pilots==1)&&(first_pilot==1)&&(((re>2)&&(re<6))||((re>8)&&(re<12)))) || \
  ((pilots==1)&&(first_pilot==0)&&(((re<3))||((re>5)&&(re<9)))) \
*/
#define is_not_pilot(pilots,first_pilot,re) (1)
/*extern void thread_top_init(char *thread_name,
		     int affinity,
		     uint64_t runtime,
		     uint64_t deadline,
		     uint64_t period);*/

void free_eNB_dlsch(LTE_eNB_DLSCH_t *dlsch)
{
  int i;
  int r;

  if (dlsch) {
#ifdef DEBUG_DLSCH_FREE
    printf("Freeing dlsch %p\n",dlsch);
#endif

    for (i=0; i<dlsch->Mdlharq; i++) {
#ifdef DEBUG_DLSCH_FREE
      printf("Freeing dlsch process %d\n",i);
#endif

      if (dlsch->harq_processes[i]) {
#ifdef DEBUG_DLSCH_FREE
        printf("Freeing dlsch process %d (%p)\n",i,dlsch->harq_processes[i]);
#endif

        if (dlsch->harq_processes[i]->b) {
          free16(dlsch->harq_processes[i]->b,MAX_DLSCH_PAYLOAD_BYTES);
          dlsch->harq_processes[i]->b = NULL;
#ifdef DEBUG_DLSCH_FREE
          printf("Freeing dlsch process %d b (%p)\n",i,dlsch->harq_processes[i]->b);
#endif
        }

#ifdef DEBUG_DLSCH_FREE
        printf("Freeing dlsch process %d c (%p)\n",i,dlsch->harq_processes[i]->c);
#endif

        for (r=0; r<MAX_NUM_DLSCH_SEGMENTS; r++) {

#ifdef DEBUG_DLSCH_FREE
          printf("Freeing dlsch process %d c[%d] (%p)\n",i,r,dlsch->harq_processes[i]->c[r]);
#endif

          if (dlsch->harq_processes[i]->c[r]) {
            free16(dlsch->harq_processes[i]->c[r],((r==0)?8:0) + 3+768);
            dlsch->harq_processes[i]->c[r] = NULL;
          }
          if (dlsch->harq_processes[i]->d[r]) {
            free16(dlsch->harq_processes[i]->d[r],(96+12+3+(3*6144)));
            dlsch->harq_processes[i]->d[r] = NULL;
          }

	}
	free16(dlsch->harq_processes[i],sizeof(LTE_DL_eNB_HARQ_t));
	dlsch->harq_processes[i] = NULL;
      }
    }

    free16(dlsch,sizeof(LTE_eNB_DLSCH_t));
    dlsch = NULL;
    }

}

LTE_eNB_DLSCH_t *new_eNB_dlsch(unsigned char Kmimo,unsigned char Mdlharq,uint32_t Nsoft,unsigned char N_RB_DL, uint8_t abstraction_flag, LTE_DL_FRAME_PARMS* frame_parms)
{

  LTE_eNB_DLSCH_t *dlsch;
  unsigned char exit_flag = 0,i,j,r,aa,layer;
  int re;
  unsigned char bw_scaling =1;

  switch (N_RB_DL) {
  case 6:
    bw_scaling =16;
    break;

  case 25:
    bw_scaling =4;
    break;

  case 50:
    bw_scaling =2;
    break;

  default:
    bw_scaling =1;
    break;
  }

  dlsch = (LTE_eNB_DLSCH_t *)malloc16(sizeof(LTE_eNB_DLSCH_t));

  if (dlsch) {
    bzero(dlsch,sizeof(LTE_eNB_DLSCH_t));
    dlsch->Kmimo = Kmimo;
    dlsch->Mdlharq = Mdlharq;
    dlsch->Mlimit = 8;
    dlsch->Nsoft = Nsoft;
    
    for (layer=0; layer<4; layer++) {
      dlsch->ue_spec_bf_weights[layer] = (int32_t**)malloc16(64*sizeof(int32_t*));
      
      for (aa=0; aa<64; aa++) {
	dlsch->ue_spec_bf_weights[layer][aa] = (int32_t *)malloc16(OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES*sizeof(int32_t));
	for (re=0;re<OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES; re++) {
	  dlsch->ue_spec_bf_weights[layer][aa][re] = 0x00007fff;
	}
      }
    }

    // NOTE: THIS HAS TO BE REVISED FOR RU, commenting to remove memory leak !!!!!
    /*
     dlsch->calib_dl_ch_estimates = (int32_t**)malloc16(frame_parms->nb_antennas_tx*sizeof(int32_t*));
     for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
       dlsch->calib_dl_ch_estimates[aa] = (int32_t *)malloc16(OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES*sizeof(int32_t));

       }*/

    for (i=0; i<10; i++)
      dlsch->harq_ids[i] = Mdlharq;

    for (i=0; i<Mdlharq; i++) {
      dlsch->harq_processes[i] = (LTE_DL_eNB_HARQ_t *)malloc16(sizeof(LTE_DL_eNB_HARQ_t));
      LOG_T(PHY, "Required mem size %d (bw scaling %d), dlsch->harq_processes[%d] %p\n",
            MAX_DLSCH_PAYLOAD_BYTES/bw_scaling,bw_scaling, i,dlsch->harq_processes[i]);

      if (dlsch->harq_processes[i]) {
        bzero(dlsch->harq_processes[i],sizeof(LTE_DL_eNB_HARQ_t));
        //    dlsch->harq_processes[i]->first_tx=1;
        dlsch->harq_processes[i]->b = (unsigned char*)malloc16(MAX_DLSCH_PAYLOAD_BYTES/bw_scaling);

        if (dlsch->harq_processes[i]->b) {
          bzero(dlsch->harq_processes[i]->b,MAX_DLSCH_PAYLOAD_BYTES/bw_scaling);
        } else {
          printf("Can't get b\n");
          exit_flag=1;
        }

        if (abstraction_flag==0) {
          for (r=0; r<MAX_NUM_DLSCH_SEGMENTS/bw_scaling; r++) {
            // account for filler in first segment and CRCs for multiple segment case
            dlsch->harq_processes[i]->c[r] = (uint8_t*)malloc16(((r==0)?8:0) + 3+ 768);
            dlsch->harq_processes[i]->d[r] = (uint8_t*)malloc16((96+12+3+(3*6144)));
            if (dlsch->harq_processes[i]->c[r]) {
              bzero(dlsch->harq_processes[i]->c[r],((r==0)?8:0) + 3+ 768);
            } else {
              printf("Can't get c\n");
              exit_flag=2;
            }
            if (dlsch->harq_processes[i]->d[r]) {
              bzero(dlsch->harq_processes[i]->d[r],(96+12+3+(3*6144)));
            } else {
              printf("Can't get d\n");
              exit_flag=2;
            }
          }
        }
      } else {
        printf("Can't get harq_p %d\n",i);
        exit_flag=3;
      }
    }

    if (exit_flag==0) {
      for (i=0; i<Mdlharq; i++) {
        dlsch->harq_processes[i]->round=0;

	for (j=0; j<96; j++)
	  for (r=0; r<MAX_NUM_DLSCH_SEGMENTS/bw_scaling; r++) {
	    //      printf("dlsch->harq_processes[%d]->d[%d] %p\n",i,r,dlsch->harq_processes[i]->d[r]);
	    if (dlsch->harq_processes[i]->d[r])
	      dlsch->harq_processes[i]->d[r][j] = LTE_NULL;
	  }

      }

      return(dlsch);
    }
  }

  LOG_D(PHY,"new_eNB_dlsch exit flag %d, size of  %ld\n",
	exit_flag, sizeof(LTE_eNB_DLSCH_t));
  free_eNB_dlsch(dlsch);
  return(NULL);


}

void clean_eNb_dlsch(LTE_eNB_DLSCH_t *dlsch)
{

  unsigned char Mdlharq;
  unsigned char i,j,r;

  if (dlsch) {
    Mdlharq = dlsch->Mdlharq;
    dlsch->rnti = 0;
    dlsch->active = 0;

    for (i=0; i<10; i++)
      dlsch->harq_ids[i] = Mdlharq;

    for (i=0; i<Mdlharq; i++) {
      if (dlsch->harq_processes[i]) {
        //  dlsch->harq_processes[i]->Ndi    = 0;
        dlsch->harq_processes[i]->status = 0;
        dlsch->harq_processes[i]->round  = 0;

	for (j=0; j<96; j++)
	  for (r=0; r<MAX_NUM_DLSCH_SEGMENTS; r++)
	    if (dlsch->harq_processes[i]->d[r])
	      dlsch->harq_processes[i]->d[r][j] = LTE_NULL;

      }
    }
  }
}




int dlsch_encoding_2threads0(te_params *tep) {

  LTE_eNB_DLSCH_t *dlsch          = tep->dlsch;
  unsigned int G                  = tep->G;
  unsigned char harq_pid          = tep->harq_pid;

  unsigned short iind;

  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;


  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_IN);

  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet

    for (r=0; r<(dlsch->harq_processes[harq_pid]->C>>2); r++) {

      if (r<dlsch->harq_processes[harq_pid]->Cminus)
        Kr = dlsch->harq_processes[harq_pid]->Kminus;
      else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
        return(-1);
      }



      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
    }

  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0; r<(dlsch->harq_processes[harq_pid]->C>>2); r++) {
    r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
                                        G,  //G
                                        dlsch->harq_processes[harq_pid]->w[r],
                                        dlsch->harq_processes[harq_pid]->e+r_offset,
                                        dlsch->harq_processes[harq_pid]->C, // C
                                        dlsch->Nsoft,                    // Nsoft,
                                        dlsch->Mdlharq,
                                        dlsch->Kmimo,
                                        dlsch->harq_processes[harq_pid]->rvidx,
                                        dlsch->harq_processes[harq_pid]->Qm,
                                        dlsch->harq_processes[harq_pid]->Nl,
                                        r,
                                        nb_rb);
    //                                        m);                       // r
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_OUT);

  return(0);
}

int dlsch_encoding_2threads1(te_params *tep) {

  LTE_eNB_DLSCH_t *dlsch          = tep->dlsch;
  unsigned int G                  = tep->G;
  unsigned char harq_pid          = tep->harq_pid;

  unsigned short iind;

  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;


  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_IN);

  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet

    for (r=(dlsch->harq_processes[harq_pid]->C>>2); r<(dlsch->harq_processes[harq_pid]->C>>1); r++) {

      if (r<dlsch->harq_processes[harq_pid]->Cminus)
        Kr = dlsch->harq_processes[harq_pid]->Kminus;
      else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
        return(-1);
      }



      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
    }

  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0,r_offset=0; r<(dlsch->harq_processes[harq_pid]->C>>1); r++) {
    if(r<(dlsch->harq_processes[harq_pid]->C>>2)){
	  int Nl=dlsch->harq_processes[harq_pid]->Nl;
      int Qm=dlsch->harq_processes[harq_pid]->Qm;
      int C = dlsch->harq_processes[harq_pid]->C;
      int Gp = G/Nl/Qm;
      int GpmodC = Gp%C;
      if (r < (C-(GpmodC)))
	    r_offset += Nl*Qm * (Gp/C);
      else
	    r_offset += Nl*Qm * ((GpmodC==0?0:1) + (Gp/C));
	}
	else{
      r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
                                          G,  //G
                                          dlsch->harq_processes[harq_pid]->w[r],
                                          dlsch->harq_processes[harq_pid]->e+r_offset,
                                          dlsch->harq_processes[harq_pid]->C, // C
                                          dlsch->Nsoft,                    // Nsoft,
                                          dlsch->Mdlharq,
                                          dlsch->Kmimo,
                                          dlsch->harq_processes[harq_pid]->rvidx,
                                          dlsch->harq_processes[harq_pid]->Qm,
                                          dlsch->harq_processes[harq_pid]->Nl,
                                          r,
                                          nb_rb);
      //                                        m);                       // r
    }
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_OUT);

  return(0);
}

int dlsch_encoding_2threads2(te_params *tep) {

  LTE_eNB_DLSCH_t *dlsch          = tep->dlsch;
  unsigned int G                  = tep->G;
  unsigned char harq_pid          = tep->harq_pid;

  unsigned short iind;

  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;


  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_IN);

  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet

    for (r=(dlsch->harq_processes[harq_pid]->C>>1); r<(dlsch->harq_processes[harq_pid]->C>>2)*3; r++) {

      if (r<dlsch->harq_processes[harq_pid]->Cminus)
        Kr = dlsch->harq_processes[harq_pid]->Kminus;
      else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
        return(-1);
      }



      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
    }

  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0,r_offset=0; r<(dlsch->harq_processes[harq_pid]->C>>2)*3; r++) {
    if(r<(dlsch->harq_processes[harq_pid]->C>>1)){
	  int Nl=dlsch->harq_processes[harq_pid]->Nl;
      int Qm=dlsch->harq_processes[harq_pid]->Qm;
      int C = dlsch->harq_processes[harq_pid]->C;
      int Gp = G/Nl/Qm;
      int GpmodC = Gp%C;
      if (r < (C-(GpmodC)))
	    r_offset += Nl*Qm * (Gp/C);
      else
	    r_offset += Nl*Qm * ((GpmodC==0?0:1) + (Gp/C));
	}
	else{
      r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
                                          G,  //G
                                          dlsch->harq_processes[harq_pid]->w[r],
                                          dlsch->harq_processes[harq_pid]->e+r_offset,
                                          dlsch->harq_processes[harq_pid]->C, // C
                                          dlsch->Nsoft,                    // Nsoft,
                                          dlsch->Mdlharq,
                                          dlsch->Kmimo,
                                          dlsch->harq_processes[harq_pid]->rvidx,
                                          dlsch->harq_processes[harq_pid]->Qm,
                                          dlsch->harq_processes[harq_pid]->Nl,
                                          r,
                                          nb_rb);
      //                                        m);                       // r
    }
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING_W, VCD_FUNCTION_OUT);

  return(0);
}

extern int oai_exit;
void *te_thread(void *param) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  
  thread_top_init("te_thread",1,200000,250000,500000);
  pthread_setname_np( pthread_self(),"te processing");
  LOG_I(PHY,"thread te created id=%ld\n", syscall(__NR_gettid));
  
  CPU_SET(4, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  PHY_VARS_eNB *eNB              = ((te_params *)param)->eNB;
  eNB_proc_t *proc               = &eNB->proc;
  
  wait_sync("te_thread");
  
  while (!oai_exit) {

    if (wait_on_condition(&proc->mutex_te[0],&proc->cond_te[0],&proc->instance_cnt_te[0],"te thread")<0) break;

    start_meas(&eNB->dlsch_turbo_encoding_wakeup_stats0);
    dlsch_encoding_2threads0((te_params*)param);
    stop_meas(&eNB->dlsch_turbo_encoding_wakeup_stats0);

    if (release_thread(&proc->mutex_te[0],&proc->instance_cnt_te[0],"te thread")<0) break;

    if (pthread_cond_signal(&proc->cond_te[0]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return(NULL);
    }
    /*if(opp_enabled == 1 && te_wakeup_stats0->diff_now>50*3000){
      print_meas_now(te_wakeup_stats0,"coding_wakeup",stderr);
      printf("te_thread0 delay for waking up in frame_rx: %d  subframe_rx: %d \n",proc->frame_rx,proc->subframe_rx);
    }*/
  }

  return(NULL);
}

void *te_thread1(void *param) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  
  thread_top_init("te_thread1",1,200000,250000,500000);
  pthread_setname_np( pthread_self(),"te processing 1");
  LOG_I(PHY,"thread te 1 created id=%ld\n", syscall(__NR_gettid));
  
  CPU_SET(7, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  PHY_VARS_eNB *eNB              = ((te_params *)param)->eNB;
  eNB_proc_t *proc               = &eNB->proc;
  
  wait_sync("te_thread1");
  
  while (!oai_exit) {


    if (wait_on_condition(&proc->mutex_te[1],&proc->cond_te[1],&proc->instance_cnt_te[1],"te thread 1")<0) break;

    start_meas(&eNB->dlsch_turbo_encoding_wakeup_stats1);
    dlsch_encoding_2threads1((te_params*)param);
    stop_meas(&eNB->dlsch_turbo_encoding_wakeup_stats1);

    if (release_thread(&proc->mutex_te[1],&proc->instance_cnt_te[1],"te thread 1")<0) break;

    if (pthread_cond_signal(&proc->cond_te[1]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread 1 exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return(NULL);
    }
    /*if(opp_enabled == 1 && te_wakeup_stats1->diff_now>50*3000){
      print_meas_now(te_wakeup_stats1,"coding_wakeup",stderr);
      printf("te_thread1 delay for waking up in frame_rx: %d  subframe_rx: %d \n",proc->frame_rx,proc->subframe_rx);
    }*/
  }

  return(NULL);
}

void *te_thread2(void *param) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  
  thread_top_init("te_thread2",1,200000,250000,500000);
  pthread_setname_np( pthread_self(),"te processing 2");
  LOG_I(PHY,"thread te 2 created id=%ld\n", syscall(__NR_gettid));
  
  CPU_SET(8, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  PHY_VARS_eNB *eNB              = ((te_params *)param)->eNB;
  eNB_proc_t *proc               = &eNB->proc;
  
  wait_sync("te_thread2");
  
  while (!oai_exit) {


    if (wait_on_condition(&proc->mutex_te[2],&proc->cond_te[2],&proc->instance_cnt_te[2],"te thread 2")<0) break;

    //stop_meas(te_wakeup_stats1);
    dlsch_encoding_2threads2((te_params*)param);


    if (release_thread(&proc->mutex_te[2],&proc->instance_cnt_te[2],"te thread 2")<0) break;

    if (pthread_cond_signal(&proc->cond_te[2]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread 1 exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return(NULL);
    }
  }

  return(NULL);
}

int dlsch_encoding_2threads(PHY_VARS_eNB *eNB,
			    unsigned char *a,
			    uint8_t num_pdcch_symbols,
			    LTE_eNB_DLSCH_t *dlsch,
			    int frame,
			    uint8_t subframe,
			    time_stats_t *rm_stats,
			    time_stats_t *te_stats,
				time_stats_t *te_wait_stats,
                time_stats_t *te_main_stats,
                time_stats_t *te_wakeup_stats0,
                time_stats_t *te_wakeup_stats1,
			    time_stats_t *i_stats)
{

  //start_meas(&eNB->dlsch_turbo_encoding_preperation_stats);

  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  eNB_proc_t *proc = &eNB->proc;
  unsigned int G;
  unsigned int crc=1;
  unsigned short iind;

  unsigned char harq_pid = dlsch->harq_ids[subframe];
  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int A;
  unsigned char mod_order;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_IN);

  A = dlsch->harq_processes[harq_pid]->TBS; //6228
  mod_order = dlsch->harq_processes[harq_pid]->Qm;
  G = get_G(frame_parms,nb_rb,dlsch->harq_processes[harq_pid]->rb_alloc,mod_order,dlsch->harq_processes[harq_pid]->Nl,num_pdcch_symbols,frame,subframe,dlsch->harq_processes[harq_pid]->mimo_mode==TM7?7:0);

  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet

    start_meas(&eNB->dlsch_turbo_encoding_preperation_stats);
    // Add 24-bit crc (polynomial A) to payload
    crc = crc24a(a,
                 A)>>8;
    a[A>>3] = ((uint8_t*)&crc)[2];
    a[1+(A>>3)] = ((uint8_t*)&crc)[1];
    a[2+(A>>3)] = ((uint8_t*)&crc)[0];

    dlsch->harq_processes[harq_pid]->B = A+24;
    memcpy(dlsch->harq_processes[harq_pid]->b,a,(A/8)+4);
    stop_meas(&eNB->dlsch_turbo_encoding_preperation_stats);

    start_meas(&eNB->dlsch_turbo_encoding_segmentation_stats);
    if (lte_segmentation(dlsch->harq_processes[harq_pid]->b,
                         dlsch->harq_processes[harq_pid]->c,
                         dlsch->harq_processes[harq_pid]->B,
                         &dlsch->harq_processes[harq_pid]->C,
                         &dlsch->harq_processes[harq_pid]->Cplus,
                         &dlsch->harq_processes[harq_pid]->Cminus,
                         &dlsch->harq_processes[harq_pid]->Kplus,
                         &dlsch->harq_processes[harq_pid]->Kminus,
                         &dlsch->harq_processes[harq_pid]->F)<0)
      return(-1);

    stop_meas(&eNB->dlsch_turbo_encoding_segmentation_stats);

    proc->tep[0].eNB               = eNB;
    proc->tep[0].dlsch             = dlsch;
    proc->tep[0].G                 = G;
    proc->tep[0].harq_pid          = harq_pid;
    
    proc->tep[1].eNB               = eNB;
    proc->tep[1].dlsch             = dlsch;
    proc->tep[1].G                 = G;
    proc->tep[1].harq_pid          = harq_pid;
    
    proc->tep[2].eNB               = eNB;
    proc->tep[2].dlsch             = dlsch;
    proc->tep[2].G                 = G;
    proc->tep[2].harq_pid          = harq_pid;
    
    start_meas(&eNB->dlsch_turbo_encoding_signal_stats);
      
    pthread_mutex_lock( &proc->mutex_te[0] );
    if (proc->instance_cnt_te[0]==0) {
      printf("[eNB] TE thread busy\n");
      exit_fun("TE thread busy");
      pthread_mutex_unlock( &proc->mutex_te[0] );
      return(-1);
    }

    ++proc->instance_cnt_te[0];

    // wakeup worker to do second half segments
    if (pthread_cond_signal(&proc->cond_te[0]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }
	//start_meas(te_wakeup_stats0);

    pthread_mutex_unlock( &proc->mutex_te[0] );
////////////////////////////////////////////////////////////////
    pthread_mutex_lock( &proc->mutex_te[1] );
    if (proc->instance_cnt_te[1]==0) {
      printf("[eNB] TE thread busy\n");
      exit_fun("TE thread busy");
      pthread_mutex_unlock( &proc->mutex_te[1] );
      return(-1);
    }

    ++proc->instance_cnt_te[1];

    // wakeup worker to do second half segments
    if (pthread_cond_signal(&proc->cond_te[1]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }
    //start_meas(te_wakeup_stats1);

    pthread_mutex_unlock( &proc->mutex_te[1] );

////////////////////////////////////////////////////////////////
    pthread_mutex_lock( &proc->mutex_te[2] );
    if (proc->instance_cnt_te[2]==0) {
      printf("[eNB] TE thread busy\n");
      exit_fun("TE thread busy");
      pthread_mutex_unlock( &proc->mutex_te[2] );
      return(-1);
    }

    ++proc->instance_cnt_te[2];

    // wakeup worker to do second half segments
    if (pthread_cond_signal(&proc->cond_te[2]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }

    pthread_mutex_unlock( &proc->mutex_te[2] );
////////////////////////////////////////////////////////////////

    stop_meas(&eNB->dlsch_turbo_encoding_signal_stats);
    start_meas(te_main_stats);
    for (r=(dlsch->harq_processes[harq_pid]->C>>2)*3; r<dlsch->harq_processes[harq_pid]->C; r++) {

      if (r<dlsch->harq_processes[harq_pid]->Cminus)
        Kr = dlsch->harq_processes[harq_pid]->Kminus;
      else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
        return(-1);
      }


      start_meas(te_stats);
      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
      stop_meas(te_stats);

      start_meas(i_stats);
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
      stop_meas(i_stats);
    }

  }
  else {

    proc->tep[0].eNB          = eNB;
    proc->tep[0].dlsch        = dlsch;
    proc->tep[0].G            = G;
	
	proc->tep[1].eNB          = eNB;
    proc->tep[1].dlsch        = dlsch;
    proc->tep[1].G            = G;
    
    proc->tep[2].eNB          = eNB;
    proc->tep[2].dlsch        = dlsch;
    proc->tep[2].G            = G;

    // wakeup worker to do second half segments
    if (pthread_cond_signal(&proc->cond_te[0]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }
	if (pthread_cond_signal(&proc->cond_te[1]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }
    if (pthread_cond_signal(&proc->cond_te[2]) != 0) {
      printf("[eNB] ERROR pthread_cond_signal for te thread exit\n");
      exit_fun( "ERROR pthread_cond_signal" );
      return (-1);
    }
  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0,r_offset=0; r<dlsch->harq_processes[harq_pid]->C; r++) {

    // get information for E for the segments that are handled by the worker thread
    if (r<(dlsch->harq_processes[harq_pid]->C>>2)*3) {
      int Nl=dlsch->harq_processes[harq_pid]->Nl;
      int Qm=dlsch->harq_processes[harq_pid]->Qm;
      int C = dlsch->harq_processes[harq_pid]->C;
      int Gp = G/Nl/Qm;
      int GpmodC = Gp%C;
      if (r < (C-(GpmodC)))
	r_offset += Nl*Qm * (Gp/C);
      else
	r_offset += Nl*Qm * ((GpmodC==0?0:1) + (Gp/C));
    }
    else  {
      start_meas(rm_stats);
      r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
					  G,  //G
					  dlsch->harq_processes[harq_pid]->w[r],
					  dlsch->harq_processes[harq_pid]->e+r_offset,
					  dlsch->harq_processes[harq_pid]->C, // C
					  dlsch->Nsoft,                    // Nsoft,
					  dlsch->Mdlharq,
					  dlsch->Kmimo,
					  dlsch->harq_processes[harq_pid]->rvidx,
					  dlsch->harq_processes[harq_pid]->Qm,
					  dlsch->harq_processes[harq_pid]->Nl,
					  r,
					  nb_rb);
      //					  m);                       // r
      stop_meas(rm_stats);
    }
  }
  stop_meas(te_main_stats);

  // wait for worker to finish
  start_meas(te_wait_stats);
  wait_on_busy_condition(&proc->mutex_te[0],&proc->cond_te[0],&proc->instance_cnt_te[0],"te thread");
  wait_on_busy_condition(&proc->mutex_te[1],&proc->cond_te[1],&proc->instance_cnt_te[1],"te thread1");
  wait_on_busy_condition(&proc->mutex_te[2],&proc->cond_te[2],&proc->instance_cnt_te[2],"te thread2");
  stop_meas(te_wait_stats);
  if(opp_enabled == 1 && te_wait_stats->diff_now>100*3000){
    print_meas_now(te_wait_stats,"coding_wait",stderr);
	printf("delay in wait on codition in frame_rx: %d \n",proc->frame_rx);
  }


  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_OUT);

  return(0);
}
int dlsch_encoding_all(PHY_VARS_eNB *eNB,
		   unsigned char *a,
                   uint8_t num_pdcch_symbols,
                   LTE_eNB_DLSCH_t *dlsch,
                   int frame,
                   uint8_t subframe,
                   time_stats_t *rm_stats,
                   time_stats_t *te_stats,
				   time_stats_t *te_wait_stats,
                   time_stats_t *te_main_stats,
                   time_stats_t *te_wakeup_stats0,
                   time_stats_t *te_wakeup_stats1,
                   time_stats_t *i_stats)
{
	int encoding_return = 0;
	unsigned int L,C,B;
	B = dlsch->harq_processes[dlsch->harq_ids[subframe]]->B;
	//printf("total bit length is %d\n",B);///////////*******
	if(B<=6144)
	{
		L=0;
		C=1;
	}
	else
	{
		L=24;
		C = B/(6144-L);
		if((6144-L)*C < B)
		{
			C = C+1;
		}
	}

	if(0/*C >= 5*/)
		{
		encoding_return =
		dlsch_encoding_2threads(eNB,
				   a,
                   num_pdcch_symbols,
                   dlsch,
                   frame,
                   subframe,
                   rm_stats,
                   te_stats,
                   te_wait_stats,
                   te_main_stats,
                   te_wakeup_stats0,
                   te_wakeup_stats1,
                   i_stats);
		}
	else
		{
		encoding_return =
		dlsch_encoding(eNB,
				   a,
                   num_pdcch_symbols,
                   dlsch,
                   frame,
                   subframe,
                   rm_stats,
                   te_stats,
                   i_stats);
		}
	return encoding_return;
}


int dlsch_encoding(PHY_VARS_eNB *eNB,
		   unsigned char *a,
                   uint8_t num_pdcch_symbols,
                   LTE_eNB_DLSCH_t *dlsch,
                   int frame,
                   uint8_t subframe,
                   time_stats_t *rm_stats,
                   time_stats_t *te_stats,
                   time_stats_t *i_stats)
{

  unsigned int G;
  unsigned int crc=1;
  unsigned short iind;

  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  unsigned char harq_pid = dlsch->harq_ids[subframe];
  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int A, Z;
  unsigned *pz = &Z;
  unsigned char mod_order;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;
  uint8_t beamforming_mode=0;
  double rate = 0.33;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_IN);

  A = dlsch->harq_processes[harq_pid]->TBS; //6228
  // printf("Encoder: A: %d\n",A);
  mod_order = dlsch->harq_processes[harq_pid]->Qm;

  if(dlsch->harq_processes[harq_pid]->mimo_mode == TM7)
    beamforming_mode = 7;
  else if(dlsch->harq_processes[harq_pid]->mimo_mode == TM8)
    beamforming_mode = 8;
  else if(dlsch->harq_processes[harq_pid]->mimo_mode == TM9_10)
    beamforming_mode = 9;
  G = get_G(frame_parms,nb_rb,dlsch->harq_processes[harq_pid]->rb_alloc,mod_order,dlsch->harq_processes[harq_pid]->Nl,num_pdcch_symbols,frame,subframe,beamforming_mode);


  //  if (dlsch->harq_processes[harq_pid]->Ndi == 1) {  // this is a new packet
  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet
#ifdef DEBUG_DLSCH_CODING
  printf("encoding thinks this is a new packet \n");
#endif
    /*
    int i;
    printf("dlsch (tx): \n");
    for (i=0;i<(A>>3);i++)
      printf("%02x.",a[i]);
    printf("\n");
    */
    // Add 24-bit crc (polynomial A) to payload
    crc = crc24a(a,
                 A)>>8;
    a[A>>3] = ((uint8_t*)&crc)[2];
    a[1+(A>>3)] = ((uint8_t*)&crc)[1];
    a[2+(A>>3)] = ((uint8_t*)&crc)[0];
    //    printf("CRC %x (A %d)\n",crc,A);

    dlsch->harq_processes[harq_pid]->B = A+24;
    //    dlsch->harq_processes[harq_pid]->b = a;
    memcpy(dlsch->harq_processes[harq_pid]->b,a,(A/8)+4);

#ifdef TD_DECODING
    if (lte_segmentation(dlsch->harq_processes[harq_pid]->b,
                         dlsch->harq_processes[harq_pid]->c,
                         dlsch->harq_processes[harq_pid]->B,
                         &dlsch->harq_processes[harq_pid]->C,
                         &dlsch->harq_processes[harq_pid]->Cplus,
                         &dlsch->harq_processes[harq_pid]->Cminus,
                         &dlsch->harq_processes[harq_pid]->Kplus,
                         &dlsch->harq_processes[harq_pid]->Kminus,
                         &dlsch->harq_processes[harq_pid]->F)<0)
      return(-1);
#else

    nr_segmentation(dlsch->harq_processes[harq_pid]->b,
		    dlsch->harq_processes[harq_pid]->c,
		    dlsch->harq_processes[harq_pid]->B,
		    &dlsch->harq_processes[harq_pid]->C,
		    &dlsch->harq_processes[harq_pid]->Kplus,
		    &dlsch->harq_processes[harq_pid]->Kminus,
		    pz,
		    &dlsch->harq_processes[harq_pid]->F);
#endif

    for (r=0; r<dlsch->harq_processes[harq_pid]->C; r++) {

      //if (r<dlsch->harq_processes[harq_pid]->Cminus)
      //  Kr = dlsch->harq_processes[harq_pid]->Kminus;
      //else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
       // return(-1);
      }


#ifdef DEBUG_DLSCH_CODING
      printf("Generating Code Segment %d (%d bits)\n",r,Kr);
      // generate codewords

      printf("bits_per_codeword (Kr)= %d, A %d\n",Kr,A);
      printf("N_RB = %d\n",nb_rb);
      printf("Ncp %d\n",frame_parms->Ncp);
      printf("mod_order %d\n",mod_order);

      printf("Encoding ... iind %d f1 %d, f2 %d\n",iind,f1f2mat_old[iind*2],f1f2mat_old[(iind*2)+1]);

      printf("start turbo encoder kr %d kr>>3 %d\n", Kr, Kr>>3);

      /*
      for (int tbc_counter = 0; tbc_counter< Kr; tbc_counter++){
	printf("turbo tbc number %d input %d\n",tbc_counter, dlsch->harq_processes[harq_pid]->c[r][tbc_counter]);
      }
      */
      printf("encoder input\n");
      for (int tbs_counter = 0; tbs_counter< Kr>>3; tbs_counter++){
	printf("c[%d][%d]=%d\n", r,tbs_counter,dlsch->harq_processes[harq_pid]->c[r][tbs_counter]);
      }
#endif

      start_meas(te_stats);
#ifdef TD_DECODING
      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
#else

#ifdef DEBUG_DLSCH_CODING
      printf("start ldpc encoder B %d, rate %f\n",dlsch->harq_processes[harq_pid]->B,rate);
      printf("input %d %d %d %d %d \n", dlsch->harq_processes[harq_pid]->c[r][0], dlsch->harq_processes[harq_pid]->c[r][1], dlsch->harq_processes[harq_pid]->c[r][2],dlsch->harq_processes[harq_pid]->c[r][3], dlsch->harq_processes[harq_pid]->c[r][4]);
#endif

      ldpc_encoder((unsigned char*)dlsch->harq_processes[harq_pid]->c[r],(unsigned char*)&dlsch->harq_processes[harq_pid]->d[r][96],Kr,rate);

#endif
      stop_meas(te_stats);

#if 0
      printf("end ldpc encoder -- output\n");

      //printf("output %d %d %d %d %d \n", dlsch->harq_processes[harq_pid]->d[r][96], dlsch->harq_processes[harq_pid]->d[r][96+1], dlsch->harq_processes[harq_pid]->d[r][96+2],dlsch->harq_processes[harq_pid]->d[r][96+3], dlsch->harq_processes[harq_pid]->d[r][96+4]);
	    
      for (int cnt =0 ; cnt < 66*128; cnt ++){
	printf("%d \n",  dlsch->harq_processes[harq_pid]->d[r][96+cnt]);
      }
      printf("\n");
#endif
 
#ifdef DEBUG_DLSCH_CODING
      if (r==0)
        write_output("enc_output0.m","enc0",&dlsch->harq_processes[harq_pid]->d[r][96],(3*8*Kr_bytes)+12,1,4);
#endif

      start_meas(i_stats);
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
      stop_meas(i_stats);
    }

  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0; r<dlsch->harq_processes[harq_pid]->C; r++) {
#ifdef DEBUG_DLSCH_CODING
    printf("Rate Matching, Code segment %d (coded bits (G) %d,unpunctured/repeated bits per code segment %d,mod_order %d, nb_rb %d)...\n",
        r,
        G,
        Kr*3,
        mod_order,nb_rb);
#endif

    start_meas(rm_stats);
#ifdef DEBUG_DLSCH_CODING
  printf("rvidx in encoding = %d\n", dlsch->harq_processes[harq_pid]->rvidx);
#endif
    r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
                                        G,  //G
                                        dlsch->harq_processes[harq_pid]->w[r],
                                        dlsch->harq_processes[harq_pid]->e+r_offset,
                                        dlsch->harq_processes[harq_pid]->C, // C
                                        dlsch->Nsoft,                    // Nsoft,
                                        dlsch->Mdlharq,
                                        dlsch->Kmimo,
                                        dlsch->harq_processes[harq_pid]->rvidx,
                                        dlsch->harq_processes[harq_pid]->Qm,
                                        dlsch->harq_processes[harq_pid]->Nl,
                                        r,
                                        nb_rb);
					//                                        m);                       // r
    stop_meas(rm_stats);
#ifdef DEBUG_DLSCH_CODING

    if (r==dlsch->harq_processes[harq_pid]->C-1)
      write_output("enc_output.m","enc",dlsch->harq_processes[harq_pid]->e,r_offset,1,4);

#endif
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_OUT);

  return(0);
}


int dlsch_encoding_SIC(PHY_VARS_UE *ue,
		       unsigned char *a,
		       uint8_t num_pdcch_symbols,
		       LTE_eNB_DLSCH_t *dlsch,
		       int frame,
		       uint8_t subframe,
		       time_stats_t *rm_stats,
		       time_stats_t *te_stats,
		       time_stats_t *i_stats)
{
  
  unsigned int G;
  unsigned int crc=1;
  unsigned short iind;

  LTE_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  unsigned char harq_pid = ue->dlsch[subframe&2][0][0]->rnti;
  unsigned short nb_rb = dlsch->harq_processes[harq_pid]->nb_rb;
  unsigned int A;
  unsigned char mod_order;
  unsigned int Kr=0,Kr_bytes,r,r_offset=0;
  //  unsigned short m=dlsch->harq_processes[harq_pid]->mcs;
  uint8_t beamforming_mode=0;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_IN);

  A = dlsch->harq_processes[harq_pid]->TBS; //6228
  // printf("Encoder: A: %d\n",A);
  mod_order = dlsch->harq_processes[harq_pid]->Qm;

  if(dlsch->harq_processes[harq_pid]->mimo_mode == TM7)
    beamforming_mode = 7;
  else if(dlsch->harq_processes[harq_pid]->mimo_mode == TM8)
    beamforming_mode = 8;
  else if(dlsch->harq_processes[harq_pid]->mimo_mode == TM9_10)
    beamforming_mode = 9;
  G = get_G(frame_parms,nb_rb,dlsch->harq_processes[harq_pid]->rb_alloc,mod_order,dlsch->harq_processes[harq_pid]->Nl,num_pdcch_symbols,frame,subframe,beamforming_mode);


  //  if (dlsch->harq_processes[harq_pid]->Ndi == 1) {  // this is a new packet
  if (dlsch->harq_processes[harq_pid]->round == 0) {  // this is a new packet
#ifdef DEBUG_DLSCH_CODING
  printf("SIC encoding thinks this is a new packet \n");
#endif
    /*
    int i;
    printf("dlsch (tx): \n");
    for (i=0;i<(A>>3);i++)
      printf("%02x.",a[i]);
    printf("\n");
    */
    // Add 24-bit crc (polynomial A) to payload
    crc = crc24a(a,
                 A)>>8;
    a[A>>3] = ((uint8_t*)&crc)[2];
    a[1+(A>>3)] = ((uint8_t*)&crc)[1];
    a[2+(A>>3)] = ((uint8_t*)&crc)[0];
    //    printf("CRC %x (A %d)\n",crc,A);

    dlsch->harq_processes[harq_pid]->B = A+24;
    //    dlsch->harq_processes[harq_pid]->b = a;
    memcpy(dlsch->harq_processes[harq_pid]->b,a,(A/8)+4);

    if (lte_segmentation(dlsch->harq_processes[harq_pid]->b,
                         dlsch->harq_processes[harq_pid]->c,
                         dlsch->harq_processes[harq_pid]->B,
                         &dlsch->harq_processes[harq_pid]->C,
                         &dlsch->harq_processes[harq_pid]->Cplus,
                         &dlsch->harq_processes[harq_pid]->Cminus,
                         &dlsch->harq_processes[harq_pid]->Kplus,
                         &dlsch->harq_processes[harq_pid]->Kminus,
                         &dlsch->harq_processes[harq_pid]->F)<0)
      return(-1);

    for (r=0; r<dlsch->harq_processes[harq_pid]->C; r++) {

      if (r<dlsch->harq_processes[harq_pid]->Cminus)
        Kr = dlsch->harq_processes[harq_pid]->Kminus;
      else
        Kr = dlsch->harq_processes[harq_pid]->Kplus;

      Kr_bytes = Kr>>3;

      // get interleaver index for Turbo code (lookup in Table 5.1.3-3 36-212, V8.6 2009-03, p. 13-14)
      if (Kr_bytes<=64)
        iind = (Kr_bytes-5);
      else if (Kr_bytes <=128)
        iind = 59 + ((Kr_bytes-64)>>1);
      else if (Kr_bytes <= 256)
        iind = 91 + ((Kr_bytes-128)>>2);
      else if (Kr_bytes <= 768)
        iind = 123 + ((Kr_bytes-256)>>3);
      else {
        printf("dlsch_coding: Illegal codeword size %d!!!\n",Kr_bytes);
        return(-1);
      }


#ifdef DEBUG_DLSCH_CODING
      printf("Generating Code Segment %d (%d bits)\n",r,Kr);
      // generate codewords

      printf("bits_per_codeword (Kr)= %d, A %d\n",Kr,A);
      printf("N_RB = %d\n",nb_rb);
      printf("Ncp %d\n",frame_parms->Ncp);
      printf("mod_order %d\n",mod_order);
#endif


#ifdef DEBUG_DLSCH_CODING
      printf("Encoding ... iind %d f1 %d, f2 %d\n",iind,f1f2mat_old[iind*2],f1f2mat_old[(iind*2)+1]);
#endif
      start_meas(te_stats);
      threegpplte_turbo_encoder(dlsch->harq_processes[harq_pid]->c[r],
                                Kr>>3,
                                &dlsch->harq_processes[harq_pid]->d[r][96],
                                (r==0) ? dlsch->harq_processes[harq_pid]->F : 0,
                                f1f2mat_old[iind*2],   // f1 (see 36121-820, page 14)
                                f1f2mat_old[(iind*2)+1]  // f2 (see 36121-820, page 14)
                               );
      stop_meas(te_stats);
#ifdef DEBUG_DLSCH_CODING

      if (r==0)
        write_output("enc_output0.m","enc0",&dlsch->harq_processes[harq_pid]->d[r][96],(3*8*Kr_bytes)+12,1,4);

#endif
      start_meas(i_stats);
      dlsch->harq_processes[harq_pid]->RTC[r] =
        sub_block_interleaving_turbo(4+(Kr_bytes*8),
                                     &dlsch->harq_processes[harq_pid]->d[r][96],
                                     dlsch->harq_processes[harq_pid]->w[r]);
      stop_meas(i_stats);
    }

  }

  // Fill in the "e"-sequence from 36-212, V8.6 2009-03, p. 16-17 (for each "e") and concatenate the
  // outputs for each code segment, see Section 5.1.5 p.20

  for (r=0; r<dlsch->harq_processes[harq_pid]->C; r++) {
#ifdef DEBUG_DLSCH_CODING
    printf("Rate Matching, Code segment %d (coded bits (G) %d,unpunctured/repeated bits per code segment %d,mod_order %d, nb_rb %d)...\n",
        r,
        G,
        Kr*3,
        mod_order,nb_rb);
#endif

    start_meas(rm_stats);
#ifdef DEBUG_DLSCH_CODING
    printf("rvidx in SIC encoding = %d\n", dlsch->harq_processes[harq_pid]->rvidx);
#endif
    r_offset += lte_rate_matching_turbo(dlsch->harq_processes[harq_pid]->RTC[r],
                                        G,  //G
                                        dlsch->harq_processes[harq_pid]->w[r],
                                        dlsch->harq_processes[harq_pid]->e+r_offset,
                                        dlsch->harq_processes[harq_pid]->C, // C
                                        dlsch->Nsoft,                    // Nsoft,
                                        dlsch->Mdlharq,
                                        dlsch->Kmimo,
                                        dlsch->harq_processes[harq_pid]->rvidx,
                                        dlsch->harq_processes[harq_pid]->Qm,
                                        dlsch->harq_processes[harq_pid]->Nl,
                                        r,
                                        nb_rb);
    //                                        m);                       // r
    stop_meas(rm_stats);
#ifdef DEBUG_DLSCH_CODING

    if (r==dlsch->harq_processes[harq_pid]->C-1)
      write_output("enc_output.m","enc",dlsch->harq_processes[harq_pid]->e,r_offset,1,4);

#endif
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_ENB_DLSCH_ENCODING, VCD_FUNCTION_OUT);

  return(0);
}





