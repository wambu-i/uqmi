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
    printf("Splitting strings!\n");
	char **result = malloc (sizeof (char *) * 3);
	char *tmp = (char *) string;
	int i = 0;

	result[i] = strtok(tmp, ",");

	while (result[i] != NULL) {
        printf("Here we go\n");
		result[++i] = strtok(NULL, ",");
	}

	return result;
}

static bool
get_sim_file_id_and_path_with_separator(const char *file_path_str, uint16_t *file_id, uint8_t **file_path, const char *separator)
{
	int i;
	char **split;

	split = split_string(file_path_str);

	if (!split) {
		fprintf(stderr, "error: invalid file path given: '%s'\n", file_path_str);
		return false;
	}

    *file_path = malloc(sizeof(uint8_t) * ARRAY_SIZE(split));

	*file_id = 0;

	for (i = 0; split[i]; i++) {
		unsigned long item = (strtoul (split[i], NULL, 16)) & 0xFFFF;
		if (split[i + 1]) {
			uint8_t val = item & 0xFF;
            (*file_path)[i] = val;
            val = (item >> 8) & 0xFF;
            (*file_path)[i + 1] = val;
        } else {
            *file_id = item;
        }
	}

	free(split);
	if (*file_id == 0) {
        free(*file_path);
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
		printf("SW1 %d\n", res.data.card_result.sw1);
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


	if (!get_sim_file_id_and_path_with_separator(arg, &id, &path, ","))
		return QMI_CMD_EXIT;

	struct qmi_uim_read_transparent_request data = {
		QMI_INIT_SEQUENCE(session_information,
			.session_type = QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
			""
		),
		QMI_INIT_SEQUENCE(file,
			.file_id = id,
			.file_path = path,
			.file_path_n = ARRAY_SIZE(&path)
		),
		QMI_INIT_SEQUENCE(read_information,
			.offset = 0,
			.length = 0
		)
	};
	qmi_set_uim_read_transparent_request(msg, &data);
	return QMI_CMD_REQUEST;
}
