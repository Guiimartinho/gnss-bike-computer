/**
 * @file ui_text.c
 * @brief Texts of the interface in Portuguese and English
 *
 * The legacy labels are English ("Dist", "Pwr", "Speed"...); the new board
 * speaks Portuguese first, with English as the second language
 * (docs/18-interface-telas.md, Fontes e textos).
 */

#include "ui_internal.h"

typedef struct {
    const char *pt;
    const char *en;
} ui_text_pair_t;

static const ui_text_pair_t texts[T_COUNT] = {
    [T_DIST] = {"Dist", "Dist"},
    [T_PWR] = {"Pot", "Pwr"},
    [T_SPEED] = {"Vel", "Speed"},
    [T_CLIMB] = {"Subida", "Climb"},
    [T_CAD] = {"Cad", "CAD"},
    [T_HR] = {"FC", "HRM"},
    [T_SLOPE] = {"Incl", "SL"},
    [T_VA] = {"VA", "VA"},
    [T_NEXT_SEG] = {"Próx. segmento", "Next segment"},
    [T_AVG] = {"Média", "Avg"},
    [T_SCORE] = {"Score", "Score"},
    [T_SOLAR] = {"Solar", "Solar"},
    [T_BATT] = {"Bat", "SOC"},
    [T_PR] = {"PR", "PR"},
    [T_NEXT_TURN] = {"Próx. curva", "Next turn"},
    [T_RR] = {"RR", "RR"},
    [T_INCLINATION] = {"Inclinação", "Pitch"},
    [T_SLOPE_HISTO] = {"Histórico da inclinação", "Pitch history"},
    [T_HEADING] = {"Rumo", "Heading"},
    [T_ROUGHNESS] = {"Rugosidade", "Roughness"},
    [T_TIME] = {"Hora", "Time"},
    [T_ELAPSED] = {"Tempo", "Time"},
    [T_ALT] = {"Alt", "Alt"},
    [T_ROUTES] = {"Percursos", "Routes"},
    [T_ERROR] = {"Erro", "Error"},
    [T_NO_ROUTES] = {"Nenhum percurso", "No route"},
    [T_PZONE] = {"Zona", "PZone"},
    [T_VECTOR] = {"Vetor de potência", "Power vector"},
    [T_CONNECTING] = {"Conectando", "Connecting"},
    [T_SEARCHING_SATS] = {"Procurando satélites", "Searching satellites"},
    [T_SATS_IN_USE] = {"satélites em uso", "satellites in use"},
    [T_MODE] = {"Modo", "Mode"},
    [T_LAST_FIX] = {"Última posição há", "Last fix"},
    [T_GNSS_BACKUP] = {"backup", "backup"},
    [T_GNSS_ACQ] = {"aquisição", "acquisition"},
    [T_GNSS_LEAP] = {"LEAP", "LEAP"},
    [T_GNSS_FULL] = {"potência plena", "full power"},
    [T_SEGMENTS_LOADED] = {"segmentos carregados", "segments loaded"},
    [T_BACK] = {"Voltar", "Back"},
    [T_MODE_FEC] = {"Modo FEC", "Mode FEC"},
    [T_MODE_CRS] = {"Modo CRS", "Mode CRS"},
    [T_MODE_PRC] = {"Modo PRC", "Mode PRC"},
    [T_MODE_ZWIFT] = {"Modo Zwift", "Mode Zwift"},
    [T_MODE_DBG] = {"Modo DBG", "Mode DBG"},
    [T_SETTINGS] = {"Ajustes", "Settings"},
    [T_SHUTDOWN] = {"Desligar", "Shutdown"},
    [T_MENU] = {"Menu", "Menu"},
    [T_SENSORS] = {"Sensores", "Sensors"},
    [T_FTP] = {"FTP", "FTP"},
    [T_WEIGHT] = {"Peso", "Weight"},
    [T_CAL_COMPASS] = {"Calibrar bússola", "Cal. mag"},
    [T_SCREEN_LIGHT] = {"Tela e luz", "Screen and light"},
    [T_GNSS] = {"GNSS", "GNSS"},
    [T_ENERGY] = {"Energia", "Energy"},
    [T_FORMAT] = {"Formatar", "Format"},
    [T_PAIR] = {"Parear", "Pair"},
    [T_CANCEL] = {"Cancelar", "Cancel"},
    [T_CENTER_PAIRS] = {"centro: parear", "center: pair"},
    [T_SAVE] = {"gravar", "save"},
    [T_LIGHT_AUTO] = {"Luz: automática", "Light: automatic"},
    [T_LIGHT_OFF] = {"Luz: desligada", "Light: off"},
    [T_THEME_COLOR] = {"Tela: cores", "Screen: colours"},
    [T_THEME_MONO] = {"Tela: preto e branco", "Screen: black and white"},
    [T_FORMAT_Q] = {"Formatar o cartão?", "Format the card?"},
    [T_FORMAT_YES] = {"Formatar e apagar", "Format and erase"},
    [T_VOLTAGE] = {"Tensão", "Voltage"},
    [T_CURRENT] = {"Corrente", "Current"},
    [T_SOURCE] = {"Fonte", "Source"},
    [T_SOLAR_LIMIT] = {"Carga solar até", "Solar charge up to"},
    [T_TEMPERATURE] = {"Temperatura", "Temperature"},
    [T_AUTONOMY] = {"Autonomia", "Autonomy"},
    [T_SRC_NONE] = {"bateria", "battery"},
    [T_SRC_SOLAR] = {"solar", "solar"},
    [T_SRC_USB] = {"USB", "USB"},
    [T_SRC_USB_FULL] = {"USB, cheia", "USB, full"},
    [T_USB_MODE] = {"Modo USB", "USB mode"},
    [T_USB_FILES] = {"arquivos no computador", "files on the computer"},
    [T_USB_UNPLUG] = {"Não desconecte", "Do not unplug"},
    [T_SAVING] = {"Salvando", "Saving"},
    [T_ACTIVITY] = {"atividade", "activity"},
    [T_SHUTTING_DOWN] = {"Desligando", "Shutting down"},
    [T_S_HR] = {"FC", "HR"},
    [T_S_BSC] = {"Vel./cad.", "Speed/cad."},
    [T_S_POWER] = {"Potência", "Power"},
    [T_S_FEC] = {"Rolo", "Trainer"},
    [T_S_RADAR] = {"Radar", "Radar"},
    [T_S_LIGHT] = {"Luz", "Light"},
    [T_L_NONE] = {"sem par", "not paired"},
    [T_L_CONNECTED] = {"conectado", "connected"},
    [T_L_LOST] = {"perdido", "lost"},
    [T_L_SEARCH] = {"procurando", "searching"},
    [T_NO_SENSOR] = {"Nenhum sensor", "No sensor"},
    [T_FIX] = {"fix", "fix"},
    [T_SATELLITES] = {"Satélites", "Satellites"},
    [T_POS_AGE] = {"Idade da posição", "Position age"},
    [T_ACCURACY] = {"Precisão", "Accuracy"},
    [T_BATTERY] = {"Bateria", "Battery"},
    [T_CHARGE] = {"Carga", "Charge"},
    [T_VERSION] = {"Versão", "Version"},
    [T_NO_ROUTE] = {"Sem percurso", "No route"},
    [T_ABOUT_H] = {"cerca de", "about"},
    [T_REMAIN] = {"Restam", "Remain"},
    [T_LIGHT] = {"Luz", "Light"},
    [T_SCREEN] = {"Tela", "Screen"},
    [T_AUTO] = {"automática", "automatic"},
    [T_OFF] = {"desligada", "off"},
    [T_COLOURS] = {"cores", "colours"},
    [T_BW] = {"preto e branco", "black and white"},
    [T_SEARCHING] = {"Procurando...", "Searching..."},
    [T_KG] = {"kg", "kg"},
    [T_W] = {"W", "W"},
    [T_SEGMENTS] = {"Segmentos", "Segments"},
};

const char *ui_txt(ui_text_t id)
{
    if ((uint32_t)id >= (uint32_t)T_COUNT) {
        return "?";
    }
    const ui_text_pair_t *t = &texts[id];
    const char *s = (ui_ctx.lang == UI_LANG_EN) ? t->en : t->pt;

    return (s != NULL) ? s : "?";
}

const char *ui_sensor_name(uint8_t kind)
{
    static const ui_text_t names[UI_SENSOR_KINDS] = {
        T_S_HR, T_S_BSC, T_S_POWER, T_S_FEC, T_S_RADAR, T_S_LIGHT,
    };

    return (kind < (uint8_t)UI_SENSOR_KINDS) ? ui_txt(names[kind]) : "?";
}
