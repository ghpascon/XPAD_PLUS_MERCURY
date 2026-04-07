#include "vars.h"
#include "serial_write_commands.h"
#include <stdio.h>

class MySerialCheck : private serial_port_write
{
public:
    void loop()
    {
        // Lê o input da serial
        String cmd = myserial.check_serial();

        if (cmd.length() > 0)
        {
            // Se for um set_cmd, envia tudo de uma vez
            if (cmd.startsWith("#set_cmd:"))
            {
                check_commands(cmd);
                return; // não continua separando
            }

            int startIndex = 0;
            int separatorIndex = cmd.indexOf('#');
            bool last_cmd = false;
            // Loop para processar todos os comandos separados por '#'
            while (true)
            {
                separatorIndex = cmd.indexOf('#', startIndex + 1);
                if (separatorIndex == -1)
                {
                    separatorIndex = cmd.length();
                    last_cmd = true;
                }
                check_commands(cmd.substring(startIndex, separatorIndex)); // Processa o comando
                startIndex = separatorIndex;
                if (last_cmd)
                    break;
            }
        }
    }

private:
    void change_password_cmd(String cmd)
    {
        if (!cmd.startsWith("#change_password:"))
        {
            myserial.write("#ERROR:Invalid command prefix");
            return;
        }

        String payload = cmd.substring(17); // remove "#change_password:"

        // Split by ';' into max 3 parts: EPC;NEW_PASSWORD;OLD_PASSWORD
        String parts[3];
        int count = 0;
        int start = 0;
        for (int i = 0; i < payload.length() && count < 3; i++)
        {
            if (payload.charAt(i) == ';')
            {
                parts[count++] = payload.substring(start, i);
                start = i + 1;
            }
        }
        parts[count++] = payload.substring(start); // last part

        // Validate number of parts (2 or 3)
        if (count < 2 || count > 3)
        {
            myserial.write("#ERROR:Invalid parameters count");
            return;
        }

        String epc = parts[0];
        String new_password = parts[1];
        String old_password = (count == 3) ? parts[2] : "00000000";

        // Validate EPC (24 hex chars)
        if (epc.length() != 24 || !validateHex(epc, 24))
        {
            myserial.write("#ERROR:Invalid EPC");
            return;
        }

        // Validate new password (8 hex chars)
        if (new_password.length() != 8 || !validateHex(new_password, 8))
        {
            myserial.write("#ERROR:Invalid new password");
            return;
        }

        // Validate old password (8 hex chars)
        if (old_password.length() != 8 || !validateHex(old_password, 8))
        {
            myserial.write("#ERROR:Invalid old password");
            return;
        }

        reader_module.change_password(epc, new_password, old_password);
    }

    void protected_mode_cmd(String cmd)
    {
        if (!cmd.startsWith("#protected_mode:"))
        {
            myserial.write("#ERROR:Invalid command prefix");
            return;
        }

        String payload = cmd.substring(16); // remove "#protected_mode:"

        // Split by ';' into 3 parts: EPC;PASSWORD;ENABLE
        String parts[3];
        int count = 0;
        int start = 0;
        for (int i = 0; i < payload.length() && count < 3; i++)
        {
            if (payload.charAt(i) == ';')
            {
                parts[count++] = payload.substring(start, i);
                start = i + 1;
            }
        }
        parts[count++] = payload.substring(start); // last part

        // Validate number of parts (must be 3)
        if (count != 3)
        {
            myserial.write("#ERROR:Missing parameters");
            return;
        }

        String epc = parts[0];
        String password = parts[1];
        String enable_str = parts[2];
        enable_str.toLowerCase();

        // Validate EPC (24 hex chars)
        if (epc.length() != 24 || !validateHex(epc, 24))
        {
            myserial.write("#ERROR:Invalid EPC");
            return;
        }

        // Validate password (8 hex chars)
        if (password.length() != 8 || !validateHex(password, 8))
        {
            myserial.write("#ERROR:Invalid password");
            return;
        }

        // Validate enable parameter
        if (enable_str != "on" && enable_str != "off" && enable_str != "true" && enable_str != "false")
        {
            myserial.write("#ERROR:Invalid enable parameter");
            return;
        }

        bool enable = (enable_str == "on" || enable_str == "true");
        // mudar a senha antes (usando senha padrão como old_password)
        reader_module.change_password(epc, password);

        reader_module.protected_mode_tag(epc, password, enable);
    }

    void protected_inventory_cmd(String cmd)
    {
        if (!cmd.startsWith("#protected_inventory:"))
        {
            myserial.write("#ERROR:Invalid command prefix");
            return;
        }

        String payload = cmd.substring(21); // remove "#protected_inventory:"

        // Split by ';' into max 2 parts: ENABLE;PASSWORD
        String parts[2];
        int count = 0;
        int start = 0;
        for (int i = 0; i < payload.length() && count < 2; i++)
        {
            if (payload.charAt(i) == ';')
            {
                parts[count++] = payload.substring(start, i);
                start = i + 1;
            }
        }
        parts[count++] = payload.substring(start); // last part

        // Validate number of parts (1 or 2)
        if (count < 1 || count > 2)
        {
            myserial.write("#ERROR:Invalid parameters count");
            return;
        }

        String enable_str = parts[0];
        enable_str.toLowerCase();
        String password = (count == 2) ? parts[1] : "00000000";

        // Validate enable parameter
        if (enable_str != "on" && enable_str != "off" && enable_str != "true" && enable_str != "false")
        {
            myserial.write("#ERROR:Invalid enable parameter");
            return;
        }

        // Password will be validated inside protected_inventory function
        bool enable = (enable_str == "on" || enable_str == "true");

        // Save configuration
        protected_inventory_enabled = enable;
        protected_inventory_password = password;

        // enable + password
        myserial.write(String(enable) + " " + password);

        reader_module.protected_inventory(enable, password);
    }

public:
    void check_commands(String cmd)
    {
        if (cmd.length() == 0)
            return;
        // ================= Comandos Gerais =================
        if (cmd == "#ping" || cmd == "ping")
        {
            myserial.write("#PONG");
        }
        else if (cmd == "#read:on" || cmd == "readtag on")
        {
            read_on = true;
            myserial.write("#READ:" + String(read_on ? "on" : "off"));
        }
        else if (cmd == "#read:off" || cmd == "readtag off")
        {
            read_on = false;
            myserial.write("#READ:" + String(read_on ? "on" : "off"));
        }
        else if (cmd == "#get_tags")
        {
            tag_commands.tag_data_display();
        }
        else if (cmd == "#get_tags_all")
        {
            tag_commands.tag_data_display_all();
        }
        else if (cmd == "#clear")
        {
            tag_commands.clear_tags();
            myserial.write("#TAGS_CLEARED");
        }
        else if (cmd.startsWith("#write:"))
        {
            write_tag_cmd(cmd);
        }
        else if (cmd.startsWith("#change_password:"))
        {
            change_password_cmd(cmd);
        }
        else if (cmd == "#get_state")
        {
            myserial.write(read_on ? "#READING" : "#IDLE");
        }
        else if (cmd == "#get_power")
        {
            myserial.write("#POWER:" + String(antena[0].power));
        }
        else if (cmd == "#get_session")
        {
            myserial.write("#SESSION:" + String(session));
        }
        else if (cmd == "#restart")
        {
            ESP.restart();
        }
        else if (cmd == "#get_serial")
        {
            myserial.write("#SERIAL:" + get_esp_name());
        }
        else if (cmd == "#get_info")
        {
            myserial.write("#NAME:" + get_esp_name());
            myserial.write("#BT_MAC:" + get_bt_mac());
            myserial.write("#ETH_MAC:" + String(ETH.macAddress()));
            myserial.write("#IP:" + ETH.localIP().toString());
        }

        // ================= Comandos de Configuração =================
        else if (cmd.startsWith("#prefix:"))
        {
            String v = cmd.substring(String("#prefix:").length());
            v.trim();
            prefix = v;
            myserial.write("#PREFIX:" + prefix);
        }
        else if (cmd == "#get_prefix")
        {
            myserial.write("#PREFIX:" + prefix);
        }

        // ================= Comandos do Set_cmd migrados =================
        else if (cmd.startsWith("#set_ant:"))
        {
            // Formato: #set_ant:n,active,power,rssi
            String ant_cmd = cmd.substring(9);
            const String sep = ",";
            int idx = ant_cmd.indexOf(sep);
            if (idx == -1)
                return;
            int current_ant = ant_cmd.substring(0, idx).toInt();
            ant_cmd = ant_cmd.substring(idx + 1);
            idx = ant_cmd.indexOf(sep);
            if (idx == -1)
                return;
            String current_active = ant_cmd.substring(0, idx);
            ant_cmd = ant_cmd.substring(idx + 1);
            idx = ant_cmd.indexOf(sep);
            if (idx == -1)
                return;
            byte current_power = ant_cmd.substring(0, idx).toInt();
            ant_cmd = ant_cmd.substring(idx + 1);
            byte current_rssi = ant_cmd.toInt();
            current_power = constrain(current_power, min_power, max_power);
            current_rssi = max(current_rssi, min_rssi);
            antena_commands.set_antena(current_ant, current_active == "on", current_power, current_rssi);
            myserial.write("#ANT_CONFIGURED:" + String(current_ant) + "," + String(current_active == "on" ? "on" : "off") + "," + String(current_power) + "," + String(current_rssi));
        }
        else if (cmd.startsWith("#session:"))
        {
            String val = cmd.substring(9);
            session = val.toInt();
            if (session > max_session)
                session = 0x00;
            myserial.write("#SESSION:" + String(session));
        }
        else if (cmd.startsWith("#read_power:"))
        {
            String val = cmd.substring(12);
            antena_commands.set_power_all(val.toInt());
            myserial.write("#POWER:" + String(val.toInt()));
        }
        else if (cmd.startsWith("#buzzer:"))
        {
            buzzer_on = cmd.endsWith("on");
            myserial.write("#BUZZER:" + String(buzzer_on ? "on" : "off"));
        }
        else if (cmd.startsWith("#gpi_stop_delay:"))
        {
            String val = cmd.substring(16);
            gpi_stop_delay = val.toInt();
        }
        else if (cmd.startsWith("#decode_gtin:"))
        {
            decode_gtin = cmd.endsWith("on");
            myserial.write("#DECODE_GTIN:" + String(decode_gtin ? "on" : "off"));
        }
        else if (cmd.startsWith("#start_reading:"))
        {
            start_reading = cmd.endsWith("on");
            read_on = start_reading;
            myserial.write("#START_READING:" + String(read_on ? "on" : "off"));
        }
        else if (cmd.startsWith("#gpi_start:"))
        {
            gpi_start = cmd.endsWith("on");
            myserial.write("#GPI_START:" + String(gpi_start ? "on" : "off"));
        }
        else if (cmd.startsWith("#always_send:"))
        {
            always_send = cmd.endsWith("on");
            myserial.write("#ALWAYS_SEND:" + String(always_send ? "on" : "off"));
        }
        else if (cmd.startsWith("#simple_send:"))
        {
            simple_send = cmd.endsWith("on");
            myserial.write("#SIMPLE_SEND:" + String(simple_send ? "on" : "off"));
        }
        else if (cmd.startsWith("#keyboard:"))
        {
            keyboard = cmd.endsWith("on");
            myserial.write("#KEYBOARD:" + String(keyboard ? "on" : "off"));
        }

        // ================= Comandos de Modo =================
        else if (cmd.startsWith("readmode"))
        {
            keyboard = cmd.endsWith("hid");
            myserial.write("#READ_MODE:" + String(keyboard ? "HID" : "USB"));
        }
        else if (cmd.startsWith("initreadtag:"))

        {
            start_reading = cmd.endsWith("on");
            read_on = start_reading;
            myserial.write("#INIT_READ_TAG:" + String(read_on ? "on" : "off"));
        }
        else if (cmd.startsWith("readpower:"))
        {
            String val = cmd.substring(10);
            antena_commands.set_power_all(val.toInt());
            myserial.write("#READ_POWER:" + String(val.toInt()));
        }
        else if (cmd.startsWith("gen2session:"))
        {
            String lastCharStr = cmd.substring(cmd.length() - 1);
            session = lastCharStr.toInt();
            if (session > max_session)
                session = 0x00;
            myserial.write("#GEN2_SESSION:" + String(session));
        }

        // ================= Comandos Protegidos =================
        else if (cmd.startsWith("#protected_mode:"))
        {
            protected_mode_cmd(cmd);
        }
        else if (cmd.startsWith("#protected_inventory:"))
        {
            protected_inventory_cmd(cmd);
            myserial.write("#PROTECTED_INVENTORY:" + String(protected_inventory_enabled ? "ENABLED" : "DISABLED"));
        }
        else if (cmd == "#get_protected_inventory")
        {
            myserial.write("#PROTECTED_INVENTORY:" + String(protected_inventory_enabled ? "ENABLED" : "DISABLED"));
            myserial.write("#PROTECTED_INVENTORY_PASSWORD:" + protected_inventory_password);
        }

        // ================= Comando set_cmd =================
        else if (cmd.startsWith("#set_cmd:"))
        {
            String cmds = cmd.substring(9);
            const String sep = "|";
            while (cmds.length() > 0)
            {
                int idx = cmds.indexOf(sep);
                String current_cmd = (idx == -1) ? cmds : cmds.substring(0, idx);
                current_cmd.trim();
                if (current_cmd.length() > 0)
                {
                    String hash_cmd = "#" + current_cmd;
                    check_commands(hash_cmd);
                }
                if (idx == -1)
                    break;
                cmds = cmds.substring(idx + 1);
            }
        }

        // ================= Setup =================
        else if (cmd == "#setup_reader")
        {
            reader_module.setup_reader();
            myserial.write("#SETUP_STARTED");
        }

        else if (cmd.startsWith("#hotspot"))
        {
            hotspot_on = cmd.endsWith("on");
            myserial.write("#HOTSPOT:" + String(hotspot_on ? "ON" : "OFF"));
        }

        // ================= GPO Commands =================
        else if (cmd.startsWith("#gpo:"))
        {
            // Formato: #gpo:n,state
            String gpo_cmd = cmd.substring(5);
            const String sep = ",";
            int idx = gpo_cmd.indexOf(sep);
            if (idx == -1)
                return;
            int gpo_index = gpo_cmd.substring(0, idx).toInt();
            String state_str = gpo_cmd.substring(idx + 1);
            state_str.toLowerCase();
            bool state = (state_str == "on" || state_str == "true");
            pins.write_gpo(gpo_index, state);
            myserial.write("#GPO:" + String(gpo_index) + "," + (state ? "ON" : "OFF"));
        }
        // ================= Fallback =================
        else
        {
            if (!check_byte_cmd(cmd))
                myserial.write("#INVALID_CMD: " + cmd);
        }
    }

    // Helper: processa um único byte_cmd e opcionalmente chama check_byte_cmd para o restante
    bool handle_byte_cmd(String byte_cmd, String rest)
    {
        myserial.write("#BYTE_CMD: " + byte_cmd);

        // SETUP e outros handlers (mantive a lógica existente)
        if (byte_cmd == "ff00031d0c")
            myserial.write_bytes("ff14030000141208003000000220220804010b0125000000107962");
        else if (byte_cmd == "ff000c1d03")
            myserial.write_bytes("ff010c0000526303");
        else if (byte_cmd == "ff00681d67")
            myserial.write_bytes("ff0168000000a4bf");
        else if (byte_cmd == "ff026a010e2e40")
            myserial.write_bytes("ff036a0000010e003044");
        else if (byte_cmd == "ff04060001c200a460")
            myserial.write_bytes("ff00060000e406");
        else if (byte_cmd == "ff026a01122e5c")
            myserial.write_bytes("ff046a00000112000069af");
        else if (byte_cmd == "ff026a01032e4d" || byte_cmd == "ff026a01032e6d")
            myserial.write_bytes("ff036a00000103003d44");
        else if (byte_cmd == "ff026a010c2e42" || byte_cmd == "ff026a010c2e62")
            myserial.write_bytes("ff036a0000010c013245");
        else if (byte_cmd == "ff026a010d2e43")
            myserial.write_bytes("ff066a0000010d000000001543");
        else if (byte_cmd == "ff00631d6c")
            myserial.write_bytes("ff0263000000052146");
        else if (byte_cmd == "ff00711d7e")
            myserial.write_bytes("ff1a7100000d0e04050608090b0c101112131415161718191a1b1c1d1e20ff3ee9");
        else if (byte_cmd == "ff00671d68")
            myserial.write_bytes("ff016700000db48c");
        else if (byte_cmd == "ff01970d4bb0")
            myserial.write_bytes("ff00970000779e");
        else if (byte_cmd == "ff026a01042e4a")
            myserial.write_bytes("ff036a00000104003a44");
        else if (byte_cmd == "ff00701d7f")
            myserial.write_bytes("ff0270000000053b75");
        else if (byte_cmd == "ff04060001c200a460")
            myserial.write_bytes("ff00060000e406");

        else if (byte_cmd == "ff016201bebc")
            myserial.write_bytes("ff07620000010a8c0a8c000054b4");
        else if (byte_cmd == "ff02940a8c2b13")
            myserial.write_bytes("ff0094000047fd");
        else if (byte_cmd == "ff00651d6a")
            myserial.write_bytes("ffc8650000000e2130000e0830000e1c80000e13e8000e1000000e1e10000e0380000e06a0000e17d0000e10c8000e1640000e0a88000e0768000e0e70000e2388000e2450000e0ce0000e1af0000e1898000e0da8000e1bb8000e1190000e0128000e14b0000e05d8000dff98000e22c0000e1960000e08f8000e21f8000e1d48000e0448000e0510000e1578000e02b8000e2518000e1ed8000e1258000e1320000e0060000e0b50000e2068000e0c18000e1a28000e01f0000e25e0000e09c0000e0f38000e1708000e1fa05ff6");
        else if (byte_cmd == "ff026b05103a7f")
            myserial.write_bytes("ff036b00000510001874");
        else if (byte_cmd == "ff026b05113a7e")
            myserial.write_bytes("ff036b00000511001974");
        else if (byte_cmd == "ff026b05023a6d")
            myserial.write_bytes("ff036b00000502020a76");
        else if (byte_cmd == "ff026b05003a6f")
            myserial.write_bytes("ff036b00000500000874");
        else if (byte_cmd == "ff026b05013a6e")
            myserial.write_bytes("ff046b0000050101002c68");
        else if (byte_cmd == "ff026b05123a7d")
            myserial.write_bytes("ff036b00000512001a74");
        else if (byte_cmd == "ff026b05163a79")
            myserial.write_bytes("ff046b0000051600024fbc");
        else if (byte_cmd == "ff026a01042e4a")
            myserial.write_bytes("ff036a00000104003a44");
        else if (byte_cmd == "ff026a01042e4a")
            myserial.write_bytes("ff036a00000104003a44");
        else if (byte_cmd == "ff00701d7f")
            myserial.write_bytes("ff0270000000053b75");
        else if (byte_cmd == "ff02100000f093")
            myserial.write_bytes("ff37100000007f01024149020200300402720908020001100b3430302d303036372d303220023041401231363233303036373932373136373236392b7393");
        else if (byte_cmd == "ff02100040f0d3")
            myserial.write_bytes("ff161000000040401231363233303036373932373136373236392b7bfc");
        else if (byte_cmd.startsWith("ff039a01"))
            myserial.write_bytes("ff009a0000a633");
        else if (byte_cmd == "ff0361000005010061df")
            myserial.write_bytes("ff0361000005010061df");
        else if (byte_cmd == "ff 03 9a 01 0c 00 a3 5d")
            myserial.write_bytes("ff 00 9a 00 00 a6 33");
        else if (byte_cmd == "ff039b050202deea")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff039b051200cee8")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff039b051600cae8")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff039b051000cce8")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff039b051100cde8")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff039b050000dce8")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff049b05010100a2fd")
            myserial.write_bytes("ff009b0000b612");
        else if (byte_cmd == "ff036c019f41897b")
            myserial.write_bytes("ff036c0000019f41869c");
        else if (byte_cmd.startsWith("ff039a01"))
            myserial.write_bytes("ff009a0000a633");
        else if (byte_cmd == "ff02060029824d")
            myserial.write_bytes("ff00060000e406");
        else if (byte_cmd == "ff00691d66")
            myserial.write_bytes("ff0169000000978e");
        else if (byte_cmd == "ff026a01082e46")
            myserial.write_bytes("ff036a00000108013645");
        else if (byte_cmd == "ff026b03103c7f")
            myserial.write_bytes("ff026b040203106774");
        else if (byte_cmd == "ff026b05143a7b")
            myserial.write_bytes("ff036b00000514001c74");
        else if (byte_cmd == "ff016104bdb9")
            myserial.write_bytes("ff71610000040180008000000002800080000000038000800000000480008000000005800080000000068000800000000780008000000008800080000000098000800000000a8000800000000b8000800000000c8000800000000d8000800000000e8000800000000f800080000000108000800000000160");

        else if (byte_cmd == "ff026b03113c7e")
            myserial.write_bytes("ff026b040203116775");
        else if (byte_cmd == "ff026a01092e47")
            myserial.write_bytes("ff036a00000109013745");
        else if (byte_cmd == "ff026a01002e4e")
            myserial.write_bytes("ff036a00000100003e44");
        else if (byte_cmd == "ff026b03123c7d")
            myserial.write_bytes("ff026b040203126776");
        else if (byte_cmd == "ff026a01062e48")
            myserial.write_bytes("ff036a00000106003844");
        else if (byte_cmd == "ff039a010400ab5d")
            myserial.write_bytes("ff009a0000a633");
        else if (byte_cmd == "ff026b05003a6f")
            myserial.write_bytes("ff036b00000500000874");
        else if (byte_cmd == "ff026c01004e88")
            myserial.write_bytes("ff026c000001000c72");
        else if (byte_cmd == "ff002a1d25")
            myserial.write_bytes("ff002a000001e8");
        else if (byte_cmd == "ff039a010c01a35c")
            myserial.write_bytes("ff009a0000a633");

        else if (byte_cmd == "ff016200bebd")
            myserial.write_bytes("ff036200000004b0af16");
        else if (byte_cmd == "ff039a010401ab5c")
            myserial.write_bytes("ff009a0000a633");
        else if (byte_cmd == "ff07910701007d02007dc568")
            myserial.write_bytes("ff009100001758");
        else if (byte_cmd == "ff016601babc")
            myserial.write_bytes("ff0d66000001010100020100030100040100fcf4");
        else if (byte_cmd == "ff039a010c00a35d")
            myserial.write_bytes("ff009a0000a633");
        else if (byte_cmd == "ff00031d0c")
            myserial.write_bytes("ff14030000141208003000000220220804010b0125000000107962");
        else if (byte_cmd == "ff0196014abc")
            myserial.write_bytes("ff02960000010128e0");
        else if (byte_cmd == "ff0196024abf")
            myserial.write_bytes("ff0296000002012be0");

        // TAGS
        else if (byte_cmd.startsWith("ff082288"))
        {
            read_on = false;
            if (current_tag == 0)
                myserial.write_bytes("ff0022040084e0");
            else
            {
                char buf[9];
                snprintf(buf, sizeof(buf), "%08x", (unsigned long)current_tag);
                String amount_cmd = String("ff0822000088000013") + String(buf);
                myserial.write_bytes(amount_cmd + myserial.get_return_crc(amount_cmd));
            }
        }
        else if (byte_cmd == "ff03290fff00facd")
        {
            current_tag--;
            String answer = "ff292900000fff00011dda110e05d80000000b00ae0500000002000000803400" + tags[current_tag].epc + "e71f";
            tags[current_tag].epc = "";
            tags[current_tag].tid = "";
            myserial.write_bytes(answer + myserial.get_return_crc(answer));
        }
        // POWER
        else if (byte_cmd.startsWith("ff0292"))
        {
            byte power = strtol(byte_cmd.substring(6, 10).c_str(), nullptr, 16) / 100;
            antena_commands.set_power_all(power);
            myserial.write_bytes("ff00920000273b");
        }

        // Reading
        else if (byte_cmd.startsWith("ff0161"))
        {
            read_on = true;
            myserial.write_bytes("ff0361000005010061df");
        }
        else if (byte_cmd == "ff026a010b2e45")
        {
            read_on = true;
            myserial.write_bytes("ff036a0000010b003544");
        }
        else if (byte_cmd.startsWith("ff132f"))
        {
            myserial.write_bytes("ff042f0000012200006dc3ff0b2200008800011b0282008200011c1d49");
            read_on = true;
        }
        else if (byte_cmd == "ff039102010142c5")
        {
            myserial.write_bytes("ff009100001758");
            read_on = true;
        }
        else if (byte_cmd == "ff8fa0d956eccaef" || byte_cmd == "ffdd082008393de9")
            read_on = true;

        else if (byte_cmd == "ff00721d7d")
        {
            read_on = false;
            myserial.write_bytes("ff017200001e4819");
        }
        else if (byte_cmd == "ff032f0000025e86")
        {
            read_on = false;
        }

        // processa o restante se houver
        if (rest.length() > 0)
            check_byte_cmd(rest);

        return true;
    }

    bool check_byte_cmd(String cmd)
    {
        // Lista de special commands: prefix + comprimento esperado (em caracteres hex)
        struct SpecialEntry
        {
            const char *prefix;
            int len; // tamanho em chars hex (ex: 26 corresponde a 13 bytes)
        };

        static const SpecialEntry specialList[] = {
            {"ff082288", 26},        // comandos que começam com ff082288 têm 26 chars
            {"ff03290fff00facd", 16} // comando completo conhecido
        };

        int specialPos = -1;
        int specialLen = 0;
        int nSpecial = sizeof(specialList) / sizeof(specialList[0]);

        for (int i = 0; i < nSpecial; i++)
        {
            int pos = cmd.indexOf(specialList[i].prefix);
            if (pos != -1 && (specialPos == -1 || pos < specialPos))
            {
                specialPos = pos;
                specialLen = specialList[i].len;
            }
        }

        if (specialPos != -1)
        {
            // Se não temos comprimento suficiente para o frame especial, aguarda mais dados
            if (cmd.length() < specialPos + specialLen)
                return false;

            String byte_cmd = cmd.substring(specialPos, specialPos + specialLen);
            String rest = (specialPos + specialLen < cmd.length()) ? cmd.substring(specialPos + specialLen) : String("");
            return handle_byte_cmd(byte_cmd, rest);
        }

        // Busca o próximo comando começando por 'ff'
        int start = cmd.indexOf("ff");
        if (start == -1)
            return true;
        int next = cmd.indexOf("ff", start + 2);
        String byte_cmd = (next == -1) ? cmd.substring(start) : cmd.substring(start, next);
        String rest = (next != -1 && next < cmd.length()) ? cmd.substring(next) : String("");
        return handle_byte_cmd(byte_cmd, rest);
    }
};
