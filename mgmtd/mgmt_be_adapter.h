// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MGMTD Backend Client Connection Adapter
 *
 * Copyright (C) 2021  Vmware, Inc.
 *		       Pushpasis Sarkar <spushpasis@vmware.com>
 */

#ifndef _FRR_MGMTD_BE_ADAPTER_H_
#define _FRR_MGMTD_BE_ADAPTER_H_

#include "mgmt_be_client.h"
#include "mgmt_msg.h"
#include "mgmtd/mgmt_defines.h"
#include "mgmtd/mgmt_ds.h"

#define MGMTD_BE_CONN_INIT_DELAY_MSEC 50

#define MGMTD_FIND_ADAPTER_BY_INDEX(adapter_index)                             \
	mgmt_adaptr_ref[adapter_index]

enum mgmt_be_req_type {
	MGMTD_BE_REQ_NONE = 0,
	MGMTD_BE_REQ_CFG_VALIDATE,
	MGMTD_BE_REQ_CFG_APPLY,
	MGMTD_BE_REQ_DATA_GET_ELEM,
	MGMTD_BE_REQ_DATA_GET_NEXT
};

struct mgmt_be_cfgreq {
	Mgmtd__YangCfgDataReq **cfgdata_reqs;
	size_t num_reqs;
};

struct mgmt_be_datareq {
	Mgmtd__YangGetDataReq **getdata_reqs;
	size_t num_reqs;
};

PREDECL_LIST(mgmt_be_adapters);
PREDECL_LIST(mgmt_txn_badapters);

struct mgmt_be_client_adapter {
	enum mgmt_be_client_id id;
	int conn_fd;
	union sockunion conn_su;
	struct event *conn_init_ev;
	struct event *conn_read_ev;
	struct event *conn_write_ev;
	struct event *conn_writes_on;
	struct event *proc_msg_ev;
	uint32_t flags;
	char name[MGMTD_CLIENT_NAME_MAX_LEN];
	uint8_t num_xpath_reg;
	char xpath_reg[MGMTD_MAX_NUM_XPATH_REG][MGMTD_MAX_XPATH_LEN];

	/* IO streams for read and write */
	struct mgmt_msg_state mstate;

	int refcount;

	/*
	 * List of config items that should be sent to the
	 * backend during re/connect. This is temporarily
	 * created and then freed-up as soon as the initial
	 * config items has been applied onto the backend.
	 */
	struct nb_config_cbs cfg_chgs;

	struct mgmt_be_adapters_item list_linkage;
	struct mgmt_txn_badapters_item txn_list_linkage;
};

#define MGMTD_BE_ADAPTER_FLAGS_WRITES_OFF (1U << 0)
#define MGMTD_BE_ADAPTER_FLAGS_CFG_SYNCED (1U << 1)

DECLARE_LIST(mgmt_be_adapters, struct mgmt_be_client_adapter, list_linkage);
DECLARE_LIST(mgmt_txn_badapters, struct mgmt_be_client_adapter,
	     txn_list_linkage);

union mgmt_be_xpath_subscr_info {
	uint8_t subscribed;
	struct {
		uint8_t validate_config : 1;
		uint8_t notify_config : 1;
		uint8_t own_oper_data : 1;
	};
};

struct mgmt_be_client_subscr_info {
	union mgmt_be_xpath_subscr_info xpath_subscr[MGMTD_BE_CLIENT_ID_MAX];
};

/* Initialise backend adapter module. */
extern int mgmt_be_adapter_init(struct event_loop *tm);

/* Destroy the backend adapter module. */
extern void mgmt_be_adapter_destroy(void);

/* Acquire lock for backend adapter. */
extern void mgmt_be_adapter_lock(struct mgmt_be_client_adapter *adapter);

/* Remove lock from backend adapter. */
extern void mgmt_be_adapter_unlock(struct mgmt_be_client_adapter **adapter);

/* Create backend adapter. */
extern struct mgmt_be_client_adapter *
mgmt_be_create_adapter(int conn_fd, union sockunion *su);

/* Fetch backend adapter given an adapter name. */
extern struct mgmt_be_client_adapter *
mgmt_be_get_adapter_by_name(const char *name);

/* Fetch backend adapter given an client ID. */
extern struct mgmt_be_client_adapter *
mgmt_be_get_adapter_by_id(enum mgmt_be_client_id id);

/* Fetch backend adapter config. */
extern int
mgmt_be_get_adapter_config(struct mgmt_be_client_adapter *adapter,
			      struct mgmt_ds_ctx *ds_ctx,
			      struct nb_config_cbs **cfg_chgs);

/* Create a transaction. */
extern int mgmt_be_create_txn(struct mgmt_be_client_adapter *adapter,
				  uint64_t txn_id);

/* Destroy a transaction. */
extern int mgmt_be_destroy_txn(struct mgmt_be_client_adapter *adapter,
				   uint64_t txn_id);

/*
 * Send config data create request to backend client.
 *
 * adaptr
 *    Backend adapter information.
 *
 * txn_id
 *    Unique transaction identifier.
 *
 * batch_id
 *    Request batch ID.
 *
 * cfg_req
 *    Config data request.
 *
 * end_of_data
 *    TRUE if the data from last batch, FALSE otherwise.
 *
 * Returns:
 *    0 on success, -1 on failure.
 */
extern int mgmt_be_send_cfg_data_create_req(
	struct mgmt_be_client_adapter *adapter, uint64_t txn_id,
	uint64_t batch_id, struct mgmt_be_cfgreq *cfg_req, bool end_of_data);

/*
 * Send config validate request to backend client.
 *
 * adaptr
 *    Backend adapter information.
 *
 * txn_id
 *    Unique transaction identifier.
 *
 * batch_ids
 *    List of request batch IDs.
 *
 * num_batch_ids
 *    Number of batch ids.
 *
 * Returns:
 *    0 on success, -1 on failure.
 */
extern int
mgmt_be_send_cfg_validate_req(struct mgmt_be_client_adapter *adapter,
				 uint64_t txn_id, uint64_t batch_ids[],
				 size_t num_batch_ids);

/*
 * Send config apply request to backend client.
 *
 * adaptr
 *    Backend adapter information.
 *
 * txn_id
 *    Unique transaction identifier.
 *
 * Returns:
 *    0 on success, -1 on failure.
 */
extern int
mgmt_be_send_cfg_apply_req(struct mgmt_be_client_adapter *adapter,
			      uint64_t txn_id);

/*
 * Dump backend adapter status to vty.
 */
extern void mgmt_be_adapter_status_write(struct vty *vty);

/*
 * Dump xpath registry for each backend client to vty.
 */
extern void mgmt_be_xpath_register_write(struct vty *vty);

/*
 * Maps a YANG dtata Xpath to one or more
 * backend clients that should be contacted for various purposes.
 */
extern int mgmt_be_get_subscr_info_for_xpath(
	const char *xpath, struct mgmt_be_client_subscr_info *subscr_info);

/*
 * Dump backend client information for a given xpath to vty.
 */
extern void mgmt_be_xpath_subscr_info_write(struct vty *vty,
					       const char *xpath);

#endif /* _FRR_MGMTD_BE_ADAPTER_H_ */
