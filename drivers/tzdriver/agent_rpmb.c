#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/memory.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <crypto/hash.h>

#include <linux/rpmb.h>

#include <linux/fs.h>
#include <linux/mmc/ioctl.h>	/* for struct mmc_ioc_rpmb */
#include <linux/mmc/card.h>	/* for struct mmc_card */
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mmc/rpmb.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sd.h>

#ifdef CONFIG_MTK_UFS_SUPPORT
#include "ufs-mtk.h"
#endif
#include <mt-plat/mtk_boot.h>

#include "core.h"
#include "mmc_ops.h"
#include "teek_client_constants.h"
/*#define TC_DEBUG*/
#include "teek_ns_client.h"
#include "agent.h"
#include "libhwsecurec/securec.h"
#include "tc_ns_log.h"
#include "smc.h"
#include "mtk_sd.h"
#include "queue.h"


#define STORAGE_IOC_MAX_RPMB_CMD 3
#define RPMB_EMMC_CID_SIZE 32
#define RPMB_CTRL_MAGIC	0x5A5A5A5A
#define RPMB_REQ               1       /* RPMB request mark */
#define RPMB_RESP              (1 << 1)/* RPMB response mark */
#define RPMB_PROGRAM_KEY       0x1    /* Program RPMB Authentication Key */
#define RPMB_GET_WRITE_COUNTER 0x2    /* Read RPMB write counter */
#define RPMB_WRITE_DATA        0x3    /* Write data to RPMB partition */
#define RPMB_READ_DATA         0x4    /* Read data from RPMB partition */
#define RPMB_RESULT_READ       0x5    /* Read result request  (Internal) */

extern struct msdc_host *mtk_msdc_host[];

typedef enum {
	sec_get_devinfo,
	sec_send_ioccmd,
	sec_rpmb_lock,
	sec_rpmb_unlock,
} rpmb_cmd_t;


struct rpmb_ioc {
	struct storage_blk_ioc_rpmb_data ioc_rpmb;  /* sizeof() = 72 */

	uint32_t buf_offset[STORAGE_IOC_MAX_RPMB_CMD];
	uint32_t tmp;
};
struct rpmb_devinfo {
	uint8_t cid[RPMB_EMMC_CID_SIZE]; /* eMMC card ID */

	uint8_t rpmb_size_mult; /* EXT CSD-slice 168 "RPMB Size" */
	uint8_t rel_wr_sec_cnt; /* EXT CSD-slice 222 "Reliable Write Sector Count" */
	uint8_t tmp[2];
	uint32_t blk_size; /* RPMB blocksize*/

	uint32_t max_blk_idx; /* The highest block index supported by current device */
	uint32_t access_start_blk_idx; /* The start block index SecureOS can access */

	uint32_t access_total_blk; /* The total blocks SecureOS can access */
	uint32_t tmp2;

	uint32_t mdt;	/* 1: EMMC 2: UFS */
	uint32_t support_bit_map;/* the device's support bit map, for example, if it support 1,2,32, then the value is 0x80000003 */
	uint32_t version; /*??16bit??0x5a5a???support_bit_map??��????16bit??��????	0x1???*/
	uint32_t tmp3;
};



struct rpmb_ctrl_t {
	uint32_t	magic;
	uint32_t	cmd_sn;

	uint8_t		lock_flag;
	uint8_t 	tmp[3];
	enum rpmb_op_type op_type;

	union __args {
		struct rpmb_devinfo get_devinfo;	/* ��С 8 * 7 */
		struct rpmb_ioc send_ioccmd;		/* ��С 8 * 11 */
	} args;


	rpmb_cmd_t	cmd;
	uint32_t 	reserved;

	uint32_t	buf_len;
	uint16_t	head_crc;
	uint16_t	buf_crc;

	int32_t		ret;
	uint32_t	reserved2;

	uint32_t buf_start[0];
};/* sizeof() = 8 * 16 = 128 */

/*lint -e754 +esym(754,*)*/

static struct rpmb_ctrl_t *m_rpmb_ctrl;

struct emmc_rpmb_blk_data {
	spinlock_t lock;
	struct device	*parent;
	struct gendisk *disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int flags;
	unsigned int usage;
	unsigned int read_only;
	unsigned int part_type;
	/* unsigned int name_idx; */
	unsigned int reset_done;

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with mmc_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int area_type;
};

int emmc_rpmb_switch(struct mmc_card *card, struct emmc_rpmb_blk_data *md)
{
	int ret;
	struct emmc_rpmb_blk_data *main_md = dev_get_drvdata(&card->dev);

	if (main_md->part_curr == md->part_type)
		return 0;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (card->ext_csd.cmdq_en) {
		ret = mmc_blk_cmdq_switch(card, 0);
		if (ret) {
			tloge("CQ disabled failed\n ret-%x", ret);
			return ret;
		}
	}
#endif

	if (mmc_card_mmc(card)) {
		u8 part_config = card->ext_csd.part_config;

		part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		part_config |= md->part_type;

		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, part_config,
				 card->ext_csd.part_time);
		if (ret)
			return ret;

		card->ext_csd.part_config = part_config;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* enable cmdq at user partition */
	if ((!card->ext_csd.cmdq_en)
	&& (md->part_type <= 0)) {
		ret = mmc_blk_cmdq_switch(card, 1);
		if (ret)
			tloge("enable CMDQ %s error %d, so just work without CMDQ\n", mmc_hostname(card->host), ret);
	}
#endif
	main_md->part_curr = md->part_type;
	return 0;
}

static int emmc_rpmb_send_command(
	struct mmc_card *card,
	struct storage_blk_ioc_rpmb_data *storage_data,
	__u16 blks,
	__u16 type,
	u8 req_type
	)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command sbc = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	u8 *transfer_buf = NULL;

	if (blks == 0) {
		tloge("Invalid blks: 0\n");
		return -EINVAL;
	}

	mrq.sbc = &sbc;
	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = NULL;
	transfer_buf = kzalloc(512 * blks, GFP_KERNEL);
	if (!transfer_buf)
		return -ENOMEM;

	/*
	 * set CMD23
	 */
	sbc.opcode = MMC_SET_BLOCK_COUNT;
	sbc.arg = blks;
	if ((req_type == RPMB_REQ && type == RPMB_WRITE_DATA) ||
					type == RPMB_PROGRAM_KEY)
		sbc.arg |= 1 << 31;
	sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

	/*
	 * set CMD25/18
	 */
	sg_init_one(&sg, transfer_buf, 512 * blks);

	if (req_type == RPMB_REQ) {

		cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;

		if (RPMB_RESULT_READ == type) {

			/* this is the step2 for write data cmd and write key cmd */
			sg_copy_from_buffer(&sg, 1, storage_data->data[1].buf, 512 * blks);

		} else {

			/* this is step 1 for read data and read counter */
			sg_copy_from_buffer(&sg, 1, storage_data->data[0].buf, 512 * blks);
		}
		data.flags |= MMC_DATA_WRITE;

	} else {

		cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		data.flags |= MMC_DATA_READ;

	}

	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	data.blksz = 512;
	data.blocks = blks;
	data.sg = &sg;
	data.sg_len = 1;

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (req_type != RPMB_REQ) {

		if (RPMB_READ_DATA == type || RPMB_GET_WRITE_COUNTER == type) {

			if (storage_data->data[1].buf != NULL)
				sg_copy_to_buffer(&sg, 1, storage_data->data[1].buf, 512 * blks);
			else
				tloge("ivalid data1buff, is null\n");

		} else if (RPMB_WRITE_DATA == type || RPMB_PROGRAM_KEY == type){

			if (storage_data->data[2].buf != NULL)
				sg_copy_to_buffer(&sg, 1, storage_data->data[2].buf, 512 * blks);
			else
				tloge("ivalid data1buff, is null\n");

		} else {
			/* do nothing */
			tloge("invalid reqtype %d\n", req_type);
		}
	}

	kfree(transfer_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}


int emmc_rpmb_req_start(struct mmc_card *card, unsigned short type, struct storage_blk_ioc_rpmb_data *storage_data)
{
	int err = 0;

	/*
	 * STEP 1: send request to RPMB partition.
	 */
	if (type == RPMB_WRITE_DATA)
		err = emmc_rpmb_send_command(card, storage_data,
						storage_data->data[0].blocks, type, RPMB_REQ);
	else {
		/* assemble the frame */
		storage_data->data[0].blocks = storage_data->data[1].blocks;

		err = emmc_rpmb_send_command(card, storage_data,
						1, type, RPMB_REQ);

	}

	if (err) {
		tloge("step 1, request failed err-%d\n", err);
		goto out;
	}

	/*
	 * STEP 2: check write result. Only for WRITE_DATA or Program key.
	 */

	if (type == RPMB_WRITE_DATA || type == RPMB_PROGRAM_KEY) {
		err = emmc_rpmb_send_command(card, storage_data,
						1, RPMB_RESULT_READ, RPMB_REQ);
		if (err) {
			tloge("step 2, request result failed err-%d\n", err);
			goto out;
		}
	}

	/*
	 * STEP 3: get response from RPMB partition
	 */

	if (type == RPMB_READ_DATA)
		err = emmc_rpmb_send_command(card, storage_data, storage_data->data[0].blocks,
						type, RPMB_RESP);
	else
		err = emmc_rpmb_send_command(card, storage_data, 1,
						type, RPMB_RESP);

	if (err)
		tloge("step 3, response failed err-%d\n", err);


out:
	return err;

}

int emmc_rpmb_req_handle(struct mmc_card *card, unsigned short type, struct storage_blk_ioc_rpmb_data *storage_data)
{
	struct emmc_rpmb_blk_data *md = NULL;
	struct emmc_rpmb_blk_data *part_md = NULL;
	int ret;

	md = dev_get_drvdata(&card->dev);

	list_for_each_entry(part_md, &md->part, part) {
		if (part_md->part_type == EXT_CSD_PART_CONFIG_ACC_RPMB)
			break;
	}

	mmc_get_card(card);

	/*
	 * STEP1: Switch to RPMB partition.
	 */
	ret = emmc_rpmb_switch(card, part_md);
	if (ret) {
		tloge("emmc_rpmb_switch failed ret-%x\n",ret);
		goto error;
	}

	/*
	 * STEP2: Start request. (CMD23, CMD25/18 procedure)
	 */
	ret = emmc_rpmb_req_start(card, type, storage_data);
	if (ret) {
		tloge("emmc_rpmb_req_start failed ret-%x\n",ret);
		goto error;
	}

error:
	ret = emmc_rpmb_switch(card, dev_get_drvdata(&card->dev));
	if (ret)
		tloge("emmc_rpmb_switch main failed ret-%x\n",ret);

	mmc_put_card(card);


	return ret;
}
/*
 * the data_ptr from SecureOS is physical address,
 * so, we MUST update to the virtual address,
 * otherwise, segment default
 */
static void update_dataptr(struct rpmb_ctrl_t *trans_ctrl)
{
	uint32_t i, offset = 0;
	uint8_t *dst = NULL;

	if (NULL == trans_ctrl)
		return;

	for (i = 0; i < STORAGE_IOC_MAX_RPMB_CMD; i++) {
		offset = trans_ctrl->args.send_ioccmd.buf_offset[i];
		if (trans_ctrl->args.send_ioccmd.ioc_rpmb.data[i].buf) {
			dst = (uint8_t *) trans_ctrl->buf_start + offset;
			/*update the data_ptr */
			trans_ctrl->args.send_ioccmd.ioc_rpmb.data[i].buf = dst;
		}
	}
}

struct rpmb_agent_lock_info {
	unsigned int dev_id;
	bool lock_need_free;
};
static struct rpmb_agent_lock_info lock_info = { 0 };

static int process_rpmb_lock(struct tee_agent_kernel_ops *agent_instance)
{
	struct smc_event_data *event_data = NULL;

	if (NULL == agent_instance)
		return -1;

	tlogd("obtain rpmb device lock\n");

	event_data = find_event_control(agent_instance->agent_id);
	if (event_data) {
		lock_info.dev_id = event_data->cmd.dev_file_id;
		lock_info.lock_need_free = true;
		tlogd("rpmb counter lock context: dev_id=%d\n",
		      lock_info.dev_id);
	}
	put_agent_event(event_data);
	return 0; /*lint !e454 */
}

static int process_rpmb_unlock(struct tee_agent_kernel_ops *agent_instance)
{
	errno_t rc = EOK;

	if (NULL == agent_instance)
		return -1;

	rc = memset_s(&lock_info, sizeof(lock_info), 0, sizeof(lock_info));/*lint !e838*/
	if (rc != EOK) {
		return -1;
	}

	lock_info.lock_need_free = false;
	tlogd("free rpmb device lock\n");
	return 0;
}

static void mtk_getrpmbdevinfo(struct rpmb_devinfo *devinfo)
{
	struct mmc_card *card = mtk_msdc_host[0]->mmc->card;

	if (devinfo == NULL || card == NULL) {
		tloge("get rpmb dev info failed\n");
		return;
	}

	devinfo->rpmb_size_mult = card->ext_csd.raw_rpmb_size_mult;

}

static int rpmb_execute_emmc(enum rpmb_op_type operation,
			       struct storage_blk_ioc_rpmb_data *storage_data)
{
	int ret;

	struct mmc_card *card = mtk_msdc_host[0]->mmc->card;

	switch (operation) {

	case RPMB_OP_RD:
		tlogd("RPMB_OP_RD\n");
		ret = emmc_rpmb_req_handle(card, RPMB_READ_DATA, storage_data);
		if (ret)
			tloge("emmc_rpmb_req_handle failed!!(%x)\n", ret);
		break;

	case RPMB_OP_WR_CNT:
		tlogd("RPMB_OP_WR_CNT\n");

		ret = emmc_rpmb_req_handle(card, RPMB_GET_WRITE_COUNTER, storage_data);
		if (ret)
			tloge("emmc_rpmb_req_handle failed ret-%x\n", ret);
		break;

	case RPMB_OP_WR_DATA:
		tlogd("RPMB_OP_WR_DATA\n");

		ret = emmc_rpmb_req_handle(card, RPMB_WRITE_DATA, storage_data);
		if (ret)
			tloge("emmc_rpmb_req_handle failed ret-%x\n", ret);
		break;

#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case RPMB_OP_KEY:
		tlogd("RPMB_OP_KEY\n");

		ret = emmc_rpmb_req_handle(card, RPMB_PROGRAM_KEY, storage_data);
		if (ret)
			tloge("emmc_rpmb_req_handle failed ret-%x\n", ret);
		break;
#endif

	default:
		tloge("receive an unknown operation %d\n", operation);
		break;

	}

	return 0;
}

static struct rpmb_frame *rpmb_alloc_frames(unsigned int cnt)
{
	return kzalloc(sizeof(struct rpmb_frame) * cnt, 0);
}

static int rpmb_req_read_data_ufs(struct storage_blk_ioc_rpmb_data *storage_data)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb = NULL;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	uint16_t blk_cnt = storage_data->data[1].blocks;
	tlogd("rpmb read data ufs, blk_cnt: %u\n", blk_cnt);

	data.req_type = RPMB_READ_DATA;
	data.icmd.nframes = 1;
	data.icmd.frames = (struct rpmb_frame *)storage_data->data[0].buf;

	/*
	 * We need to fill-in block_count by ourselves for UFS case.
	 */
	data.icmd.frames->block_count = cpu_to_be16(blk_cnt);

	data.ocmd.nframes = blk_cnt;
	data.ocmd.frames = (struct rpmb_frame *)storage_data->data[1].buf;

	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);
	if (ret != 0)
		tloge("rpmb_req_read_data_ufs error, ret:0x%x\n",ret);

	tlogd(" result 0x%x\n", cpu_to_be16(data.ocmd.frames->result));

	return ret;
}

static int rpmb_req_write_data_ufs(struct storage_blk_ioc_rpmb_data *storage_data)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb = NULL;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	uint16_t blk_cnt = storage_data->data[0].blocks;

	tlogd("blk_cnt: %d\n", blk_cnt);

	/*
	 * Alloc output frame to avoid overwriting input frame
	 * buffer provided by TEE
	 */
	data.ocmd.frames = rpmb_alloc_frames(1);

	if (data.ocmd.frames == NULL)
		return RPMB_ALLOC_ERROR;

	data.ocmd.nframes = 1;

	data.req_type = RPMB_WRITE_DATA;
	data.icmd.nframes = blk_cnt;
	data.icmd.frames = (struct rpmb_frame *)storage_data->data[0].buf;


	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);
	if (ret)
		tloge("rpmb_req write_data_ufs error, ret:0x%x\n",ret);

	/*
	 * Microtrust TEE will check write counter in the first frame,
	 * thus we copy response frame to the first frame.
	 */
	ret = memcpy_s(storage_data->data[2].buf, storage_data->data[2].buf_bytes,
				   data.ocmd.frames, sizeof(*(data.ocmd.frames)));
	if (ret != EOK)
		tloge("frames cpoy fail, ret:0x%x", ret);

	tlogd("result 0x%x\n", cpu_to_be16(data.ocmd.frames->result));

	kfree(data.ocmd.frames);

	return ret;
}

static int rpmb_req_get_wc_ufs(u8 *key, u32 *wc, struct storage_blk_ioc_rpmb_data *storage_data)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb = NULL;
	u8 nonce[RPMB_SZ_NONCE] = {0};
	u8 hmac[RPMB_SZ_MAC] = {0};
	int ret, i;

	tlogd("rpmb_req_get_wc_ufs start!!!\n");

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	do {
		/*
		 * Initial frame buffers
		 */
		if (storage_data != NULL) {

			/*
			 * Use external frame if possible.
			 * External frame shall have below field ready,
			 *
			 * nonce
			 * req_resp
			 */
			data.icmd.frames = (struct rpmb_frame *)storage_data->data[0].buf;
			data.ocmd.frames = (struct rpmb_frame *)storage_data->data[1].buf;

		} else {

			data.icmd.frames = rpmb_alloc_frames(1);

			if (data.icmd.frames == NULL)
				return RPMB_ALLOC_ERROR;

			data.ocmd.frames = rpmb_alloc_frames(1);

			if (data.ocmd.frames == NULL) {
				kfree(data.icmd.frames);
				return RPMB_ALLOC_ERROR;
			}
		}

		/*
		 * Prepare frame contents.
		 *
		 * Input frame (in view of device) only needs nonce
		 */
		data.req_type = RPMB_GET_WRITE_COUNTER;
		data.icmd.nframes = 1;

		/* Fill-in essential field in self-prepared frame */
		if (storage_data == NULL) {
			get_random_bytes(nonce, RPMB_SZ_NONCE);
			data.icmd.frames->req_resp = cpu_to_be16(RPMB_GET_WRITE_COUNTER);
			ret = memcpy_s(data.icmd.frames->nonce, sizeof(data.icmd.frames->nonce), nonce, RPMB_SZ_NONCE);
			if (ret != EOK)
				tloge("copy nonce fail");
		}

		/* Output frame (in view of device) */
		data.ocmd.nframes = 1;

		ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

		if (ret != 0) {
			tloge("rpmb_req_get_wc_ufs error!!! ret:0x%x\n",ret);
			break;
		}

		/* Verify HMAC only if key is available */
		if (key != NULL) {
		/*
		 * Authenticate response write counter frame.
		 */
			if (memcmp(hmac, data.ocmd.frames->key_mac, RPMB_SZ_MAC) != 0) {
				tloge("hmac compare error!!!\n");
				ret = RPMB_HMAC_ERROR;
			}

		/*
		 * DEVICE ISSUE:
		 * We found some devices will return hmac vale with all
		 * zeros.
		 * For this kind of device, bypass hmac comparison.
		 */
		if (ret == RPMB_HMAC_ERROR) {
			for (i = 0; i < 32; i++) {
				if (data.ocmd.frames->key_mac[i] != 0x0) {
					tloge("device hmac is not NULL!!!\n");
					break;
				}
			}

			tloge("device hmac has all zero, bypassed!!!\n");
			ret = RPMB_SUCCESS;
		}
		}

		/*
		 * Verify nonce and result only in self-prepared frame
		 * External frame shall be verified by frame provider,
		 * for example, TEE.
		 */
		if (storage_data == NULL) {
			if (memcmp(nonce, data.ocmd.frames->nonce,
				RPMB_SZ_NONCE) != 0) {
				tloge("nonce compare error!!!\n");
				ret = RPMB_NONCE_ERROR;
				break;
			}

			if (data.ocmd.frames->result) {
				tloge("result error!!! 0x%x\n", cpu_to_be16(data.ocmd.frames->result));
				ret = RPMB_RESULT_ERROR;
				break;
			}
		}

		if (wc) {
			*wc = cpu_to_be32(data.ocmd.frames->write_counter);
			tlogd("wc = %d ,0x%x\n", *wc, *wc);
		}
	} while (0);

	tlogd("end\n");

	if (storage_data == NULL) {
		kfree(data.icmd.frames);
		kfree(data.ocmd.frames);
	}

	return ret;
}


#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
static int rpmb_req_program_key_ufs(struct storage_blk_ioc_rpmb_data *storage_data)
{
	struct rpmb_data data;
	struct rpmb_dev *rawdev_ufs_rpmb = NULL;
	int ret;

	rawdev_ufs_rpmb = ufs_mtk_rpmb_get_raw_dev();

	/*
	 * Alloc output frame to avoid overwriting input frame
	 * buffer provided by TEE
	 */
	data.ocmd.frames = rpmb_alloc_frames(1);

	if (data.ocmd.frames == NULL)
		return RPMB_ALLOC_ERROR;

	data.ocmd.nframes = 1;

	data.req_type = RPMB_PROGRAM_KEY;
	data.icmd.nframes = 1;
	data.icmd.frames = (struct rpmb_frame *)storage_data->data[0].buf;

	ret = rpmb_cmd_req(rawdev_ufs_rpmb, &data);

	if (ret != 0)
		tloge("rpmb_req_program_key_ufs error, ret:0x%x\n",ret);

	/*
	 * Microtrust TEE will check write counter in the first frame,
	 * thus we copy response frame to the first frame.
	 */
	ret = memcpy_s(storage_data->data[2].buf, storage_data->data[2].buf_bytes,
				   data.ocmd.frames, sizeof(*(data.ocmd.frames)));
	if (ret != EOK)
		tloge("copy frame fail!");

	if (data.ocmd.frames->result != 0) {
		tloge("result error!!! result:0x%x \n", cpu_to_be16(data.ocmd.frames->result));
		ret = RPMB_RESULT_ERROR;
	}

	kfree(data.ocmd.frames);

	return ret;
}
#endif


#ifdef CONFIG_MTK_UFS_SUPPORT
static int rpmb_execute_ufs(enum rpmb_op_type operation,
								 struct storage_blk_ioc_rpmb_data *storage_data)
{
   int ret;
   tlogd("rpmb_execute_ufs enter");

	switch (operation) {

	case RPMB_OP_RD:
		ret = rpmb_req_read_data_ufs(storage_data);
		break;
	case RPMB_OP_WR_CNT:
		ret = rpmb_req_get_wc_ufs(NULL, NULL, storage_data);
		break;
	case RPMB_OP_WR_DATA:
		ret = rpmb_req_write_data_ufs(storage_data);
		break;
#ifdef CFG_RPMB_KEY_PROGRAMED_IN_KERNEL
	case RPMB_OP_KEY:
		ret = rpmb_req_program_key_ufs(storage_data);
	break;
#endif

	default:
		tloge("receive an unknown command id %d.\n", operation);
		break;
	}

	return ret;
}
#endif



int mtk_rpmb_ioctl_cmd(int id, enum rpmb_op_type operation,
				   struct storage_blk_ioc_rpmb_data *storage_data)
{
	int ret = 0;
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_SDMMC)
		ret = rpmb_execute_emmc(operation, storage_data);
#ifdef CONFIG_MTK_UFS_SUPPORT
	else if (boot_type == BOOTDEV_UFS)
		ret = rpmb_execute_ufs(operation, storage_data);
#endif
	return ret;
}

#define GET_RPMB_LOCK_MASK 0x01
#define FREE_RPMB_LOCK_MASK 0x02
static void send_ioccmd(struct tee_agent_kernel_ops *agent_instance)
{
	uint8_t lock_flag;
	int32_t ret;

	if (NULL == agent_instance || NULL == m_rpmb_ctrl) {
		tloge("bad parameters\n");
		return;
	}

	lock_flag = m_rpmb_ctrl->lock_flag;


	if (lock_flag & GET_RPMB_LOCK_MASK)
		process_rpmb_lock(agent_instance);

	ret = mtk_rpmb_ioctl_cmd(0, m_rpmb_ctrl->op_type,
				  &m_rpmb_ctrl->args.send_ioccmd.ioc_rpmb);

	if (ret)
		tloge("rpmb ioctl failed: %d\n", ret);

	if (lock_flag & FREE_RPMB_LOCK_MASK)
		process_rpmb_unlock(agent_instance);
	m_rpmb_ctrl->ret = ret;
}

static int rpmb_check_data(struct rpmb_ctrl_t *trans_ctrl)
{
	uint16_t obj_crc;
	size_t buf_crc_start_offset;

	if (NULL == trans_ctrl)
		return 0;

	if (RPMB_CTRL_MAGIC != trans_ctrl->magic) {
		tloge("rpmb check magic error, now is 0x%x\n", trans_ctrl->magic);
		return -1;
	}

	return 0;
}
static uint32_t m_cmd_sn;
u64  g_ioctl_start_time = 0;
u64  g_ioctl_end_time = 0;
struct timeval tv;


static int rpmb_agent_work(struct tee_agent_kernel_ops *agent_instance)
{
	struct rpmb_ctrl_t *trans_ctrl = NULL;
	errno_t rc = EOK;
	uint32_t copy_len;

	if (NULL == agent_instance || NULL == agent_instance->agent_buff)
		return -1;

	trans_ctrl = (struct rpmb_ctrl_t *)agent_instance->agent_buff;

	if (0 == rpmb_check_data(trans_ctrl)) {

		if (m_cmd_sn != trans_ctrl->cmd_sn) {
			if (m_cmd_sn) {

				tv = ns_to_timeval(g_ioctl_end_time - g_ioctl_start_time);
				tlogd("cmd_sn %x, total cost %d.%d s\n", trans_ctrl->cmd_sn, (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec);

			}

			m_cmd_sn = trans_ctrl->cmd_sn;
		}

		if (NULL == m_rpmb_ctrl) {
			m_rpmb_ctrl = kzalloc(agent_instance->agent_buff_size, GFP_KERNEL);
			if (NULL == m_rpmb_ctrl) {
				tloge("memory alloc failed\n");
				trans_ctrl->ret = TEEC_ERROR_OUT_OF_MEMORY;
				return -1;
			}

		}
		rc = memcpy_s((void *)m_rpmb_ctrl, agent_instance->agent_buff_size,
			      (void *)trans_ctrl, sizeof(struct rpmb_ctrl_t) + trans_ctrl->buf_len);
		if (EOK != rc) {
			tloge("memcpy_s failed: 0x%x\n", rc);
			trans_ctrl->ret = TEEC_ERROR_SECURITY;
			kfree(m_rpmb_ctrl);
			m_rpmb_ctrl = NULL;
			return -1;
		}

		update_dataptr(m_rpmb_ctrl);

		switch (m_rpmb_ctrl->cmd) {
		case sec_get_devinfo:	/* stb used */
			tlogd("rpmb agent cmd is get_devinfo\n");
			// from Chicago this function not supported yet
			mtk_getrpmbdevinfo(&m_rpmb_ctrl->args.get_devinfo);
			m_rpmb_ctrl->ret = 0;
			break;
		case sec_send_ioccmd:
			tlogd("rpmb agent cmd is send ioc\n");
			send_ioccmd(agent_instance);
			break;
		case sec_rpmb_lock:
			tlogd("rpmb agent cmd is lock\n");
			process_rpmb_lock(agent_instance);
			m_rpmb_ctrl->ret = 0;
			break;
		case sec_rpmb_unlock:
			tlogd("rpmb agent cmd is unlock\n");
			process_rpmb_unlock(agent_instance);
			m_rpmb_ctrl->ret = 0;
			break;
		default:
			tloge("rpmb agent cmd not supported 0x%x\n", m_rpmb_ctrl->cmd);
			break;
		}

		copy_len = agent_instance->agent_buff_size
			   - offsetof(struct rpmb_ctrl_t, buf_start);
		rc = memcpy_s((void *)trans_ctrl->buf_start, copy_len,
			      (void *)m_rpmb_ctrl->buf_start, copy_len);
		if (EOK != rc) {
			tloge("memcpy_s 2 failed: 0x%x\n", rc);
			trans_ctrl->ret = TEEC_ERROR_SECURITY;
			kfree(m_rpmb_ctrl);
			m_rpmb_ctrl = NULL;
			return -1;
		}

		trans_ctrl->ret = m_rpmb_ctrl->ret;


	} else {
		trans_ctrl->ret = TEEC_ERROR_BAD_FORMAT;
		return -1;
	}

	return 0;
}

static int rpmb_agent_exit(struct tee_agent_kernel_ops *agent_instance)
{
	tloge("rpmb agent is exit is being invoked\n");

	if (NULL != m_rpmb_ctrl) {
		kfree(m_rpmb_ctrl);
		m_rpmb_ctrl = NULL;
	}

	return 0;
}


static int rpmb_agent_crash_work(struct tee_agent_kernel_ops *agent_instance,
	tc_ns_client_context *context,
	unsigned int dev_file_id)
{
	tlogd("check free lock or not, dev_id=%d\n", dev_file_id);
	if (lock_info.lock_need_free && (lock_info.dev_id == dev_file_id)) {
		tloge("CA crash, need to free lock\n");
		process_rpmb_unlock(agent_instance);
	}
	return 0;
}


static struct tee_agent_kernel_ops rpmb_agent_ops = {
	.agent_name = "rpmb",
	.agent_id = TEE_RPMB_AGENT_ID,
	.tee_agent_init = NULL,
	.tee_agent_work = rpmb_agent_work,
	.tee_agent_exit = rpmb_agent_exit,
	.tee_agent_crash_work = rpmb_agent_crash_work,

	.list = LIST_HEAD_INIT(rpmb_agent_ops.list)
};
/*lint -restore*/

int rpmb_agent_register(void)
{

	tee_agent_kernel_register(&rpmb_agent_ops);

	return 0;
}

EXPORT_SYMBOL(rpmb_agent_register);
