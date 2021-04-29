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

#define cmd_uim_verify_pin1_cb no_cb
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

static char **split_string(const char *string) {
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

static stack *create_stack() {
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

static bool push(stack *stack, uint8_t val) {
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

static void
cmd_uim_get_iccid_cb(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg)
{
	struct qmi_uim_read_transparent_response res;
	qmi_parse_uim_read_transparent_response(msg, &res);

	if (res.set.card_result) {
		int len = res.data.read_result_n;
		char tmp[MAX];
		char result[len * 2];
		memset(result, 0, len * 2);
		for (int i = 0; i < len; i++) {
			sprintf(tmp, "%02X", res.data.read_result[i]);
			printf("Tmp - %s\n", tmp);
			strcat(result, tmp);
		}
		printf("ICCID is %s\n", result);
	}
	else {
		printf("Error getting information!\n");
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

	printf("Len of arguments %d\n", new->len);

	path = new->args;
	for (int i = 0; i < new->len; i++) {
                printf("Argument at %d is %d\n", i, path[i]);
        }

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
	qmi_parse_uim_read_transparent_response(msg, &res);

	if (res.set.card_result) {
		char tmp[5];
		int len = res.data.read_result_n;
		for (int i = 0; i < len; i++) {
			sprintf(tmp, "%02X", res.data.read_result[i]);
			printf("Tmp - %s\n", tmp);
		}
		printf("SW1 %d\n", res.data.card_result.sw1);
	}
	else {
		printf("Error getting information!\n");
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

	printf("Len of arguments %d\n", new->len);

	path = new->args;
	for (int i = 0; i < new->len; i++) {
		printf("Argument at %d is %d\n", i, path[i]);
	}

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

#define cmd_uim_read_transparent_cb no_cb
static enum qmi_cmd_result
cmd_uim_read_transparent_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	uint16_t id = 0;
	uint8_t *path = NULL;

	stack *new = create_stack();

	if (!get_sim_file_id_and_path_with_separator(arg, &id, new, ","))
		return QMI_CMD_EXIT;

	printf("Len of arguments %d\n", new->len);

	path = new->args;
	for (int i = 0; i < new->len; i++) {
		printf("Argument at %d is %d\n", i, path[i]);
	}

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
	printf("PIN is provided as %s\n", arg);
	uim_req_data.pin = arg;
	return QMI_CMD_DONE;
}

static enum qmi_cmd_result
cmd_uim_set_pin_protection_prepare(struct qmi_msg *msg, char *arg)
{
	if (!uim_req_data.pin) {
		uqmi_add_error("Sucks - Missing argument");
		return QMI_CMD_EXIT;
	}
	printf("Arg is %s, pin is %s, and id is %d\n", arg, uim_req_data.pin, uim_req_data.pin_id);

	int is_enabled;
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
			.pin_id = uim_req_data.pin_id
		),
		QMI_INIT_PTR(info.old_pin, uim_req_data.pin),
		QMI_INIT_PTR(info.new_pin, uim_req_data.new_pin)
	};

	qmi_set_uim_change_pin_request(msg, &uim_change_pin_req);
	return QMI_CMD_REQUEST;
}

#define cmd_uim_change_pin1_cb no_cb
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

static enum qmi_cmd_result
cmd_uim_get_pin_status_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	qmi_set_uim_get_pin_status_request(msg);
	return QMI_CMD_REQUEST;
}

#define cmd_uim_verify_pin1_cb no_cb
static enum qmi_cmd_result
cmd_uim_verify_pin1_prepare(struct qmi_dev *qmi, struct qmi_request *req, struct qmi_msg *msg, char *arg)
{
	printf("PIN is given as %s\n", arg);
	struct qmi_uim_verify_pin_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_UIM_PIN_ID_PIN,
			.pin = arg
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
			.application_identifier = "{}"
		),
		QMI_INIT_SEQUENCE(info,
			.pin_id = QMI_UIM_PIN_ID_PIN2,
			.pin = arg
		)
	};
	qmi_set_uim_verify_pin_request(msg, &data);
	return QMI_CMD_REQUEST;
}