/*
 * tee_rdr_register.c
 *
 * for rdr memory register
 *
 * Copyright (c) 2012-2018 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "tee_rdr_register.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <securec.h>
#include "teek_client_constants.h"
#include "tc_ns_client.h"
#include "teek_ns_client.h"
#include "tc_ns_log.h"
#include "smc.h"
#include "mailbox_mempool.h"

#ifdef DEF_ENG
#define KERNEL_IMG_IS_ENG 1
#endif
struct rdr_register_module_result {
  u64   log_addr;
  u32   log_len;
};

struct rdr_register_module_result current_rdr_info;

/* Register rdr memory */
int tc_ns_register_rdr_mem(void)
{
	tc_ns_smc_cmd smc_cmd = { {0}, 0 };
	int ret = 0;
	u64 rdr_mem_addr;
	unsigned int rdr_mem_len;
	struct mb_cmd_pack *mb_pack = NULL;

	current_rdr_info.log_addr  = (uintptr_t)__get_free_pages(GFP_KERNEL, RDR_MEM_ORDER_MAX);
	if (!current_rdr_info.log_addr) {
  		tloge("fail to alloc rdr mem\n");
		return -ENOMEM;
        }
	current_rdr_info.log_len = RDR_MEM_SIZE;

	rdr_mem_addr = virt_to_phys((uintptr_t)current_rdr_info.log_addr);
	rdr_mem_len = current_rdr_info.log_len;

	mb_pack = mailbox_alloc_cmd_pack();
	if (mb_pack == NULL) {
		current_rdr_info.log_addr = 0x0;
		current_rdr_info.log_len = 0;
		return -ENOMEM;
	}

	smc_cmd.global_cmd = true;
	smc_cmd.cmd_id = GLOBAL_CMD_ID_REGISTER_RDR_MEM;
	mb_pack->operation.paramtypes = TEE_PARAM_TYPE_VALUE_INPUT |
		TEE_PARAM_TYPE_VALUE_INPUT << TEE_PARAM_NUM;
	mb_pack->operation.params[0].value.a = rdr_mem_addr;
	mb_pack->operation.params[0].value.b = rdr_mem_addr >> ADDR_TRANS_NUM;
	mb_pack->operation.params[1].value.a = rdr_mem_len;
#ifdef DEF_ENG
	mb_pack->operation.params[1].value.b = KERNEL_IMG_IS_ENG;
#endif

	smc_cmd.operation_phys = virt_to_phys(&mb_pack->operation);
	smc_cmd.operation_h_phys = virt_to_phys(&mb_pack->operation) >> ADDR_TRANS_NUM;
	ret = tc_ns_smc(&smc_cmd);
	mailbox_free(mb_pack);
	if (ret)
		tloge("Send rdr mem info failed.\n");
	return ret;
}

unsigned long tc_ns_get_rdr_mem_addr(void)
{
	return current_rdr_info.log_addr;
}

unsigned int tc_ns_get_rdr_mem_len(void)
{
	return current_rdr_info.log_len;
}
