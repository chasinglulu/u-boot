// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 xinlu.wang
 */

#include <common.h>
#include <dm.h>
#include <scsi.h>
#include <virtio_types.h>
#include <virtio.h>
#include "dm/device.h"
#include "linux/dma-direction.h"
#include <virtio_ring.h>
#include "virtio_scsi.h"

struct virtio_scsi_priv {
	struct virtqueue *vq;
};

struct virtio_scsi_cmd {
    struct scsi_cmd *sc;
	union {
		struct virtio_scsi_cmd_req       cmd;
		struct virtio_scsi_cmd_req_pi    cmd_pi;
		struct virtio_scsi_ctrl_tmf_req  tmf;
		struct virtio_scsi_ctrl_an_req   an;
	} req;
	union {
		struct virtio_scsi_cmd_resp      cmd;
		struct virtio_scsi_ctrl_tmf_resp tmf;
		struct virtio_scsi_ctrl_an_resp  an;
		struct virtio_scsi_event         evt;
	} resp;
};

static int virtio_scsi_bus_reset(struct udevice *dev)
{
	/* Not implemented */
	return 0;
}

static int virtio_scsi_do_req(struct virtqueue *vq,
			    struct virtio_scsi_cmd *cmd,
			    size_t req_size, size_t resp_size)
{
    struct scsi_cmd *sc = cmd->sc;
    struct virtio_sg *sgs[3];
    unsigned int num_out = 0, num_in = 0;
    struct virtio_scsi_cmd_resp *resp = &cmd->resp.cmd;
	int ret;

    struct virtio_sg req_sg = { &cmd->req, req_size };
	struct virtio_sg data_sg = { sc->pdata, sc->datalen };
	struct virtio_sg resp_sg = { &cmd->resp, resp_size };

    sgs[num_out++] = &req_sg;
    if (sc->dma_dir == DMA_TO_DEVICE)
		sgs[num_out++] = &data_sg;

    sgs[num_out + num_in++] = &resp_sg;
    if (sc->dma_dir == DMA_FROM_DEVICE)
	    sgs[num_out + num_in++] = &data_sg;

    ret = virtqueue_add(vq, sgs, num_out, num_in);
	if (ret)
		return ret;

	virtqueue_kick(vq);

	while (!virtqueue_get_buf(vq, NULL))
		;


	printf("cmd %p response %u status %#02x sense_len %u resid 0x%x\n",
		sc, resp->response, resp->status, resp->sense_len, resp->resid);
	return cmd->resp.cmd.status;
}

static int virtio_scsi_exec(struct udevice *dev, struct scsi_cmd *pccb)
{
    printf("%s\n", __func__);
    struct virtio_scsi_cmd cmd;
    struct virtio_scsi_cmd_req *cmd_req;
    struct virtio_scsi_priv *priv = dev_get_priv(dev);
    u8 status;
    int i;

    memset(&cmd, 0, sizeof(cmd));

    cmd.sc = pccb;

    cmd_req = &cmd.req.cmd;

    cmd_req->lun[0] = 1;
    cmd_req->lun[1] = pccb->target;
    cmd_req->lun[2] = (pccb->lun >> 8) | 0x40;
    cmd_req->lun[3] = pccb->lun & 0xff;
    cmd_req->tag = cpu_to_virtio64(dev, (unsigned long)pccb);
    cmd_req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    cmd_req->prio = 0;
    cmd_req->crn = 0;
    memcpy(cmd_req->cdb, pccb->cmd, pccb->cmdlen);

	printf("lun:\n");
	for (i = 0; i < 8; i++)
		printf("%u ", cmd_req->lun[i]);
	printf("\ntag: 0x%llx\n", cmd_req->tag);
	printf("task_attr: %u\n", cmd_req->task_attr);
	printf("prio: %u crn: %u\n", cmd_req->prio, cmd_req->crn);
	printf("cdb:\n");
	for (i = 0; i < VIRTIO_SCSI_CDB_SIZE; i++)
		printf("%u ", cmd_req->cdb[i]);
	printf("\n");

    status = virtio_scsi_do_req(priv->vq, &cmd, sizeof(cmd.req.cmd), sizeof(cmd.resp.cmd));
    printf("%s: staus %d\n", __func__, status);
    return 0;
}

struct scsi_ops virtio_scsi_ops = {
	.exec		= virtio_scsi_exec,
	.bus_reset	= virtio_scsi_bus_reset,
};

static int virtio_scsi_probe(struct udevice *dev)
{
    printf("%s\n", __func__);
    struct virtio_scsi_priv *priv = dev_get_priv(dev);
    struct scsi_plat *uc_plat = dev_get_uclass_plat(dev);
    u16 num_targets;
    u32 num_luns, max_blks;
    int ret;

	ret = virtio_find_vqs(dev, 1, &priv->vq);
	if (ret)
		return ret;

    virtio_cread(dev, struct virtio_scsi_config, max_target, &num_targets);
    virtio_cread(dev, struct virtio_scsi_config, max_lun, &num_luns);
    virtio_cread(dev, struct virtio_scsi_config, max_sectors, &max_blks);

    printf("%s: num_targets 0x%x\n", __func__, num_targets);
    printf("%s: num_luns 0x%x\n", __func__, num_luns);
    printf("%s: max_blks 0x%x\n", __func__, max_blks);

    uc_plat->max_lun = num_luns;
    uc_plat->max_id = num_targets;

    return 0;
}

U_BOOT_DRIVER(virtio_scsi) = {
	.name		= VIRTIO_SCSI_DRV_NAME,
	.id         = UCLASS_SCSI,
	.ops		= &virtio_scsi_ops,
    .probe      = virtio_scsi_probe,
    .priv_auto	= sizeof(struct virtio_scsi_priv),
};