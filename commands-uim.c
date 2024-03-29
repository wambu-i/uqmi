/*
 * uqmi -- tiny QMI support implementation
 *
 * Copyright (C) 2016 Felix Fietkau <nbd@nbd.name>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#define MAX 10

typedef struct stack stack;

struct stack {
	int top;
	int len;
	uint8_t *args;
};

static struct {
	QmiUimPinId pin_id;
	char* pin;
	char* new_pin;
	char* puk;
} uim_req_data;

static void
cmd_uim_verify_pin1_cb (struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_verify_pin_response res;

	qmi_parse_uim_verify_pin_response(msg, &res);

	if (res.set.card_result) {
		blobmsg_add_string(&status, NULL, "PIN verified successfully.");
	}
}

static enum qmi_cmd_result
cmd_uim_verify_pin1_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	struct qmi_uim_verify_pin_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
			""
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_UIM_PIN_ID_PIN1,
			.pin_value = arg
		)
	};
	qmi_set_uim_verify_pin_request(msg, &data);
	return QMI_CMD_REQUEST;
}

#define cmd_uim_verify_pin2_cb no_cb
static enum qmi_cmd_result
cmd_uim_verify_pin2_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	struct qmi_uim_verify_pin_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
			""
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_UIM_PIN_ID_PIN2,
			.pin_value = arg
		)
	};
	qmi_set_uim_verify_pin_request(msg, &data);
	return QMI_CMD_REQUEST;
}

static char **split_string(const char *string)
{
	char **result = malloc (sizeof (char *) * MAX);
	char *tmp = (char *) string;
	int i = 0;
	char *token;

	token = strtok(tmp, ",");

	while (token != NULL) {
		result[i++] = token;
		token = strtok(NULL, ",");
	}

	return result;
}

static void reverse_string(char* str)
{
	int len = strlen(str);

	for (int i = 0, j = len - 1; i < j; i++, j--) {
		char ch = str[i];
		str[i] = str[j];
		str[j] = ch;
	}
}

static stack *create_stack()
{
	stack *new = malloc(sizeof(*new));
	if (!new) {
		fprintf(stderr, "Could not allocate new memory!");
		return NULL;
	}

	new->top = -1;
	new->len = 0;
	new->args = malloc(sizeof(uint8_t) * MAX);

	return new;
}

static bool push(stack *stack, uint8_t val)
{
	if (stack->top == MAX - 1) return false;

	stack->args[++stack->top] = val;
	stack->len++;
	return true;
}

static bool
get_sim_file_id_and_path_with_separator(const char *file_path_str, uint16_t *file_id, stack *file_stack, const char *separator)
{
	int i;
	char **split;

	split = split_string(file_path_str);

	if (!split) {
		fprintf(stderr, "error: invalid file path given: '%s'\n", file_path_str);
		return false;
	}

	*file_id = 0;

	for (i = 0; split[i]; i++) {
		unsigned long item = (strtoul (split[i], NULL, 16)) & 0xFFFF;
		if (split[i + 1]) {
			uint8_t val = item & 0xFF;
			push(file_stack, val);
			val = (item >> 8) & 0xFF;
			push(file_stack, val);
		} else {
			*file_id = item;
		}
	}

	free(split);
	if (*file_id == 0) {
		fprintf(stderr, "error: invalid file path given: '%s'\n", file_path_str);
		return false;
	}

	return true;
}

static char *read_raw_data(int len, uint8_t *read_result, bool reversed)
{
	char tmp[MAX];
	char *result = malloc(sizeof(char) * len * 2);
	memset(result, 0, len * 2);
	for (int i = 0; i < len; i++) {
		sprintf(tmp, "%02X", read_result[i]);
		if (reversed) reverse_string(tmp);
		strcat(result, tmp);
	}

	return result;
}

static void
cmd_uim_get_iccid_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_read_transparent_response res;
	char *result;

	qmi_parse_uim_read_transparent_response(msg, &res);

	if (res.set.card_result) {
		result = read_raw_data(res.data.read_result_n, res.data.read_result, true);
		blobmsg_add_string(&status, NULL, result);
	}
}

static enum qmi_cmd_result
cmd_uim_get_iccid_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uint16_t id = 0;
	uint8_t *path = NULL;
	stack *new = create_stack();

	if (!get_sim_file_id_and_path_with_separator(arg, &id, new, ","))
		return QMI_CMD_EXIT;

	path = new->args;

	struct qmi_uim_read_transparent_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(file,
			.file_id = id,
			.file_path = path,
			.file_path_n = new->len
		),
		QMI_INIT_SEQUENCE(read_information,
			.offset = 0,
			.length = 0
		)
	};

	qmi_set_uim_read_transparent_request(msg, &data);

	free(new);
	return QMI_CMD_REQUEST;
}

static void
cmd_uim_get_imsi_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_read_transparent_response res;
	char *result;

	qmi_parse_uim_read_transparent_response(msg, &res);

	if (res.set.card_result) {
		result = read_raw_data(res.data.read_result_n, res.data.read_result, false);
		blobmsg_add_string(&status, NULL, result);
	}
}

static enum qmi_cmd_result
cmd_uim_get_imsi_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uint16_t id = 0;
	uint8_t *path = NULL;

	stack *new = create_stack();

	if (!get_sim_file_id_and_path_with_separator(arg, &id, new, ","))
		return QMI_CMD_EXIT;

	path = new->args;

	struct qmi_uim_read_transparent_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(file,
			.file_id = id,
			.file_path = path,
			.file_path_n = new->len
		),
		QMI_INIT_SEQUENCE(read_information,
			.offset = 0,
			.length = 0
		)
	};

	qmi_set_uim_read_transparent_request(msg, &data);

	free(new);
	return QMI_CMD_REQUEST;
}

static void
cmd_uim_read_transparent_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_read_transparent_response res;
	char *result;

	qmi_parse_uim_read_transparent_response(msg, &res);

	if (res.set.card_result) {
		result = read_raw_data(res.data.read_result_n, res.data.read_result, false);
		blobmsg_add_string(&status, NULL, result);
	}
}

static enum qmi_cmd_result
cmd_uim_read_transparent_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uint16_t id = 0;
	uint8_t *path = NULL;

	stack *new = create_stack();

	if (!get_sim_file_id_and_path_with_separator(arg, &id, new, ","))
		return QMI_CMD_EXIT;

	path = new->args;

	struct qmi_uim_read_transparent_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(file,
			.file_id = id,
			.file_path = path,
			.file_path_n = new->len
		),
		QMI_INIT_SEQUENCE(read_information,
			.offset = 0,
			.length = 0
		)
	};

	qmi_set_uim_read_transparent_request(msg, &data);

	free(new);
	return QMI_CMD_REQUEST;
}

#define cmd_uim_set_pin_cb no_cb
static enum qmi_cmd_result
cmd_uim_set_pin_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.pin = arg;
	return QMI_CMD_DONE;
}

static enum qmi_cmd_result
cmd_uim_set_pin_protection_prepare(struct qmi_msg *msg, char *arg)
{
	if (!uim_req_data.pin) {
		uqmi_add_error("Missing argument");
		return QMI_CMD_EXIT;
	}

	bool is_enabled;
	if (strcasecmp(arg, "disabled") == 0)
		is_enabled = false;
	else if (strcasecmp(arg, "enabled") == 0)
		is_enabled = true;
	else {
		uqmi_add_error("Invalid value (valid: disabled, enabled)");
		return QMI_CMD_EXIT;
	}

	struct qmi_uim_set_pin_protection_request request = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = uim_req_data.pin_id,
			.pin_value = uim_req_data.pin,
			.pin_enabled = is_enabled
		)
	};

	qmi_set_uim_set_pin_protection_request(msg, &request);
	return QMI_CMD_REQUEST;
}

#define cmd_uim_set_pin1_protection_cb no_cb
static enum qmi_cmd_result
cmd_uim_set_pin1_protection_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.pin_id = QMI_UIM_PIN_ID_PIN1;
	return cmd_uim_set_pin_protection_prepare(msg, arg);
}

#define cmd_uim_set_pin2_protection_cb no_cb
static enum qmi_cmd_result
cmd_uim_set_pin2_protection_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.pin_id = QMI_UIM_PIN_ID_PIN2;
	return cmd_uim_set_pin_protection_prepare(msg, arg);
}

static enum qmi_cmd_result
cmd_uim_change_pin_prepare(struct qmi_msg *msg, char *arg)
{
	if (!uim_req_data.pin || !uim_req_data.new_pin) {
		uqmi_add_error("Missing argument");
		return QMI_CMD_EXIT;
	}

	struct qmi_uim_change_pin_request uim_change_pin_req = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = uim_req_data.pin_id,
			.old_pin = uim_req_data.pin,
			.new_pin = uim_req_data.new_pin
		)
	};

	qmi_set_uim_change_pin_request(msg, &uim_change_pin_req);
	return QMI_CMD_REQUEST;
}

static void cmd_uim_change_pin1_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg) {
	struct qmi_uim_change_pin_response res;

	qmi_parse_uim_change_pin_response(msg, &res);
	if (res.set.card_result) {
		blobmsg_add_string(&status, NULL, "PIN1 changed successfully.");
	}
}
static enum qmi_cmd_result
cmd_uim_change_pin1_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.pin_id = QMI_UIM_PIN_ID_PIN1;
	return cmd_uim_change_pin_prepare(msg, arg);
}

#define cmd_uim_change_pin2_cb no_cb
static enum qmi_cmd_result
cmd_uim_change_pin2_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.pin_id = QMI_UIM_PIN_ID_PIN2;
	return cmd_uim_change_pin_prepare(msg, arg);
}

#define cmd_uim_set_new_pin_cb no_cb
static enum qmi_cmd_result
cmd_uim_set_new_pin_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uim_req_data.new_pin = arg;
	return QMI_CMD_DONE;
}

static const char *qmi_uim_get_pin_status(int status)
{
	static const char *card_status[] = {
		[QMI_UIM_PIN_STATE_NOT_INITIALIZED] = "not-initialized",
		[QMI_UIM_PIN_STATE_ENABLED_NOT_VERIFIED] = "enabled-not-verified",
		[QMI_UIM_PIN_STATE_ENABLED_VERIFIED] = "verified",
		[QMI_UIM_PIN_STATE_DISABLED] = "disabled",
		[QMI_UIM_PIN_STATE_BLOCKED] = "blocked",
		[QMI_UIM_PIN_STATE_PERMANENTLY_BLOCKED] = "permanently-blocked",
	};
	const char *res = "Unknown";

	if (status < ARRAY_SIZE(card_status) && card_status[status])
		res = card_status[status];

	return res;
}

static const char *qmi_uim_get_card_status(int status) {
	static const char *card_status[] = {
		[QMI_UIM_CARD_STATE_ABSENT]  = "absent",
		[QMI_UIM_CARD_STATE_PRESENT] = "present",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(card_status) && card_status[status])
		res = card_status[status];

	return res;
}

static const char *qmi_uim_get_card_error_string(int status) {
	static const char *card_error[] = {
		[QMI_UIM_CARD_ERROR_UNKNOWN] = "unknown-error",
		[QMI_UIM_CARD_ERROR_POWER_DOWN] = "power-down",
		[QMI_UIM_CARD_ERROR_POLL] = "poll-error",
		[QMI_UIM_CARD_ERROR_NO_ATR_RECEIVED] = "no-ATR-received",
		[QMI_UIM_CARD_ERROR_VOLTAGE_MISMATCH] = "voltage-mismatch",
		[QMI_UIM_CARD_ERROR_PARITY] = "parity-error",
		[QMI_UIM_CARD_ERROR_POSSIBLY_REMOVED] = "unknown-error-possibly-removed",
		[QMI_UIM_CARD_ERROR_TECHNICAL] = "technical-problem",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(card_error) && card_error[status])
		res = card_error[status];

	return res;
}

static const char *qmi_uim_get_application_type_string(int status) {
	static const char *application_type[] = {
		[QMI_UIM_CARD_APPLICATION_TYPE_UNKNOWN] = "unknown",
		[QMI_UIM_CARD_APPLICATION_TYPE_SIM]     = "sim",
		[QMI_UIM_CARD_APPLICATION_TYPE_USIM]    = "usim",
		[QMI_UIM_CARD_APPLICATION_TYPE_RUIM]    = "ruim",
		[QMI_UIM_CARD_APPLICATION_TYPE_CSIM]    = "csim",
		[QMI_UIM_CARD_APPLICATION_TYPE_ISIM]    = "isim",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(application_type) && application_type[status])
		res = application_type[status];

	return res;
}

static const char *qmi_uim_get_application_state_string(int status) {
	static const char *application_state[] = {
		[QMI_UIM_CARD_APPLICATION_STATE_UNKNOWN]               	     = "unknown",
		[QMI_UIM_CARD_APPLICATION_STATE_DETECTED]                    = "detected",
		[QMI_UIM_CARD_APPLICATION_STATE_PIN1_OR_UPIN_PIN_REQUIRED]   = "pin1-or-upin-pin-required",
		[QMI_UIM_CARD_APPLICATION_STATE_PUK1_OR_UPIN_PUK_REQUIRED]   = "puk1-or-upin-required",
		[QMI_UIM_CARD_APPLICATION_STATE_CHECK_PERSONALIZATION_STATE] = "personalization-state-must-be-checked",
		[QMI_UIM_CARD_APPLICATION_STATE_PIN1_BLOCKED]                = "pin1-blocked",
		[QMI_UIM_CARD_APPLICATION_STATE_ILLEGAL]                     = "illegal",
		[QMI_UIM_CARD_APPLICATION_STATE_READY]                       = "ready",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(application_state) && application_state[status])
		res = application_state[status];

	return res;
}

static const char *qmi_uim_get_personalization_state_string(int status) {
	static const char *personalization_state[] = {
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_UNKNOWN]             = "unknown",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_IN_PROGRESS]         = "in-progress",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_READY]               = "ready",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_CODE_REQUIRED]       = "code-required",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_PUK_CODE_REQUIRED]   = "puk-code-required",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_PERMANENTLY_BLOCKED] = "permanently-blocked",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(personalization_state) && personalization_state[status])
		res = personalization_state[status];

	return res;
}

static const char *qmi_uim_get_personalization_feature_string(int status) {
	static const char *personalization_feature[] = {
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK]          = "gw-network",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_NETWORK_SUBSET]   = "gw-network-subset",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_SERVICE_PROVIDER] = "gw-service-provider",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_CORPORATE]        = "gw-corporate",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_GW_UIM]              = "gw-uim",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_NETWORK_TYPE_1]   = "1x-network-type-1",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_NETWORK_TYPE_2]   = "1x-network-type-2",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_HRPD]             = "1x-hrpd",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_SERVICE_PROVIDER] = "1x-service-provider",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_CORPORATE]        = "1x-corporate",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_1X_RUIM]             = "1x-ruim",
		[QMI_UIM_CARD_APPLICATION_PERSONALIZATION_FEATURE_UNKNOWN]             = "unknown",
	};

	const char *res = "Unknown";

	if (status < ARRAY_SIZE(personalization_feature) && personalization_feature[status])
		res = personalization_feature[status];

	return res;
}

static void cmd_uim_get_card_status_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_get_card_status_response res;
	void *c, *slots, *slot, *application, *applications, *pin1, *pin2;
	int state;

	qmi_parse_uim_get_card_status_response(msg, &res);

	c = blobmsg_open_table(&status, NULL);
	slots = blobmsg_open_array(&status, "slots");

	if (res.set.card_status) {
		for (int i = 0; i < res.data.card_status.cards_n; i++) {
			slot = blobmsg_open_table(&status, NULL);
			blobmsg_add_u32(&status, "Slot ", (int32_t) i + 1);
			state = res.data.card_status.cards[i].card_state;
			if (state != QMI_UIM_CARD_STATE_ERROR)
				blobmsg_add_string(&status, "Card State", qmi_uim_get_card_status(state));
			else
				blobmsg_add_string(&status, "Card Error", qmi_uim_get_card_error_string(state));
			blobmsg_add_string(&status, "UPIN State", qmi_uim_get_pin_status(res.data.card_status.cards[i].upin_state));
			blobmsg_add_u32(&status, "UPIN retries", (int32_t) res.data.card_status.cards[i].upin_retries);
			blobmsg_add_u32(&status, "UPUK retries", (int32_t) res.data.card_status.cards[i].upuk_retries);

			applications = blobmsg_open_array(&status, "applications");

			for (int j = 0; j < res.data.card_status.cards->applications_n; j++) {
				application = blobmsg_open_table(&status, NULL);
				blobmsg_add_u32(&status, "Application", (int32_t) j + 1);
				blobmsg_add_string(&status, "Application type", qmi_uim_get_application_type_string(res.data.card_status.cards[i].applications[j].type));
				blobmsg_add_string(&status, "Application state", qmi_uim_get_application_state_string(res.data.card_status.cards[i].applications[j].state));
				blobmsg_add_string(&status, "Application ID", read_raw_data(res.data.card_status.cards[i].applications[i].application_identifier_value_n, res.data.card_status.cards[i].applications[j].application_identifier_value, false));
				if (res.data.card_status.cards[i].applications[j].personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_CODE_REQUIRED ||
					res.data.card_status.cards[i].applications[j].personalization_state == QMI_UIM_CARD_APPLICATION_PERSONALIZATION_STATE_PUK_CODE_REQUIRED) {
					blobmsg_add_string(&status, "Personalization state", qmi_uim_get_personalization_state_string(res.data.card_status.cards[i].applications[j].personalization_state));
					blobmsg_add_string(&status, "Personalization feature", qmi_uim_get_personalization_feature_string(res.data.card_status.cards[i].applications[j].personalization_feature));
					blobmsg_add_u32(&status, "Disable retries", (int32_t) res.data.card_status.cards[i].applications[j].personalization_retries);
					blobmsg_add_u32(&status, "Unblock retries", (int32_t) res.data.card_status.cards[i].applications[j].personalization_unblock_retries);
					}
				else {
					blobmsg_add_string(&status, "Personalization state", qmi_uim_get_personalization_state_string(res.data.card_status.cards[i].applications[j].personalization_state));
				}
				blobmsg_add_string(&status, "UPIN replaces PIN1", res.data.card_status.cards[i].applications[j].upin_replaces_pin1 ? "yes" : "no");

				blobmsg_add_string(&status, "PIN1 state", qmi_uim_get_pin_status(res.data.card_status.cards[i].applications[j].pin1_state));
				pin1 = blobmsg_open_table(&status, NULL);
				blobmsg_add_u32(&status, "PIN1 retries", (int32_t) res.data.card_status.cards[i].applications[j].pin1_retries);
				blobmsg_add_u32(&status, "PUK1 retries", (int32_t) res.data.card_status.cards[i].applications[j].puk1_retries);
				blobmsg_close_table(&status, pin1);

				blobmsg_add_string(&status, "PIN2 state", qmi_uim_get_pin_status(res.data.card_status.cards[i].applications[j].pin2_state));
				pin2 = blobmsg_open_table(&status, NULL);
				blobmsg_add_u32(&status, "PIN2 retries", (int32_t) res.data.card_status.cards[i].applications[j].pin2_retries);
				blobmsg_add_u32(&status, "PUK2 retries", (int32_t) res.data.card_status.cards[i].applications[j].puk2_retries);
				blobmsg_close_table(&status, pin2);

				blobmsg_close_table(&status, application);
			}
			blobmsg_close_array(&status, applications);
			blobmsg_close_table(&status, slot);
		}
		blobmsg_close_array(&status, slots);
	}
	blobmsg_close_table(&status, c);
}

static enum qmi_cmd_result
cmd_uim_get_card_status_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_uim_get_card_status_request(msg);
	return QMI_CMD_REQUEST;
}

static void cmd_uim_get_pin1_info_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_get_card_status_response res;
	void *c, *pin1, *pin2;

	qmi_parse_uim_get_card_status_response(msg, &res);

	c = blobmsg_open_table(&status, NULL);

	if (res.set.card_status) {
		for (int i = 0; i < res.data.card_status.cards_n; i++) {
			for (int j = 0; j < res.data.card_status.cards->applications_n; j++) {
				const char *type = qmi_uim_get_application_type_string(res.data.card_status.cards[i].applications[j].type);

				if (strcmp(type, "usim") == 0) {
					blobmsg_add_string(&status, "PIN1 state", qmi_uim_get_pin_status(res.data.card_status.cards[i].applications[j].pin1_state));
					pin1 = blobmsg_open_table(&status, NULL);
					blobmsg_add_u32(&status, "PIN1 retries", (int32_t) res.data.card_status.cards[i].applications[j].pin1_retries);
					blobmsg_add_u32(&status, "PUK1 retries", (int32_t) res.data.card_status.cards[i].applications[j].puk1_retries);
					blobmsg_close_table(&status, pin1);

					blobmsg_add_string(&status, "PIN2 state", qmi_uim_get_pin_status(res.data.card_status.cards[i].applications[j].pin2_state));
					pin2 = blobmsg_open_table(&status, NULL);
					blobmsg_add_u32(&status, "PIN2 retries", (int32_t) res.data.card_status.cards[i].applications[j].pin2_retries);
					blobmsg_add_u32(&status, "PUK2 retries", (int32_t) res.data.card_status.cards[i].applications[j].puk2_retries);
					blobmsg_close_table(&status, pin2);
				}
			}
		}
	}
	blobmsg_close_table(&status, c);
}

static enum qmi_cmd_result
cmd_uim_get_pin1_info_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_uim_get_card_status_request(msg);
	return QMI_CMD_REQUEST;
}