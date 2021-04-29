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

#define __uqmi_uim_commands												\
	__uqmi_command(uim_verify_pin1, uim-verify-pin1, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_verify_pin2, uim-verify-pin2, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_get_iccid, uim-get-iccid, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_get_imsi, uim-get-imsi, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_read_transparent, uim-read-transparent, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_set_pin1_protection, uim-set-pin1-protection, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_set_pin2_protection, uim-set-pin2-protection, required, QMI_SERVICE_UIM), \
	__uqmi_command(uim_set_pin, uim-pin, required, CMD_TYPE_OPTION), \
	__uqmi_command(uim_change_pin1, uim-change-pin1, no, QMI_SERVICE_UIM), \
	__uqmi_command(uim_change_pin2, uim-change-pin2, no, QMI_SERVICE_UIM), \
	__uqmi_command(uim_set_new_pin, uim-new-pin, required, CMD_TYPE_OPTION) \
	__uqmi_command(uim_get_card_status, uim_get_card_status, no, QMI_SERVICE_UIM) \

#define uim_helptext \
		"  --uim-verify-pin1 <pin>:          Verify PIN1 (new devices)\n" \
		"  --uim-verify-pin2 <pin>:          Verify PIN2 (new devices)\n" \
		"  --uim-get-iccid <file path>:      Get ICCID from SIM using specified file path\n" \
		"  --uim-get-imsi <file path>:       Get IMSI from SIM using specified file path\n" \
		"  --uim-read-transparent <file path>: Read from specific SIM file path\n" \
		"  --uim-set-pin1-protection <state>: Set PIN1 protection state (disabled, enabled)\n" \
		"    --uim-pin <pin>:                PIN1 needed to change state\n" \
		"  --uim-set-pin2-protection <state>: Set PIN2 protection state (disabled, enabled)\n" \
		"    --uim-pin <pin2>:                PIN2 needed to change state\n" \
		"  --uim-change-pin1:                     Change PIN1\n" \
		"    --uim-pin <old pin>:                 Current PIN1\n" \
		"    --uim-new-pin <new pin>:             New pin\n" \
		"  --uim-change-pin2:                     Change PIN2\n" \
		"    --uim-pin <old pin>:                 Current PIN2\n" \
		"    --uim-new-pin <new pin>:             New pin\n" \
		"  --uim-get-card-status:				  Get Card Status"
