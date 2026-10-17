#include "strings.h"

#include <windows.h>

static const wchar_t *const english[STR_COUNT] = {
    L"MouseFix: active (%u ms)",
    L"MouseFix: paused",
    L"MouseFix is running",
    L"MouseFix is paused",
    L"Click the mouse icon next to the clock to configure or exit. If you "
    L"can't see it, click the ^ arrow next to the clock.",
    L"MouseFix is running and already fixing double clicks.\n\n"
    L"Should it start automatically every time Windows starts? "
    L"(recommended)\n\n"
    L"You can change this later in the menu of the mouse icon next to the "
    L"clock.",
    L"Could not install the mouse hook.",
    L"Phantom clicks blocked: %lu",
    L"Enabled",
    L"Interval",
    L"%u ms (default)",
    L"Buttons",
    L"Left",
    L"Right",
    L"Middle (wheel)",
    L"Side 1 (back)",
    L"Side 2 (forward)",
    L"Mode",
    L"Block repeated click (default)",
    L"Drag-safe (delays the release)",
    L"Start with Windows",
    L"Exit",
};

static const wchar_t *const portuguese[STR_COUNT] = {
    L"MouseFix: ativo (%u ms)",
    L"MouseFix: pausado",
    L"MouseFix está funcionando",
    L"MouseFix está pausado",
    L"Clique no ícone do mouse perto do relógio para configurar ou sair. Se "
    L"ele não aparecer, clique na setinha ^ ao lado do relógio.",
    L"O MouseFix está funcionando e já está corrigindo os cliques duplos.\n\n"
    L"Deseja que ele abra sozinho sempre que o Windows iniciar? "
    L"(recomendado)\n\n"
    L"Você pode mudar isso depois no menu do ícone do mouse, perto do "
    L"relógio.",
    L"Não foi possível instalar o hook do mouse.",
    L"Cliques fantasmas bloqueados: %lu",
    L"Ativado",
    L"Intervalo",
    L"%u ms (padrão)",
    L"Botões",
    L"Esquerdo",
    L"Direito",
    L"Meio (roda)",
    L"Lateral 1 (voltar)",
    L"Lateral 2 (avançar)",
    L"Modo",
    L"Bloquear clique repetido (padrão)",
    L"Seguro para arrastar (atrasa o soltar)",
    L"Iniciar com o Windows",
    L"Sair",
};

static const wchar_t *const *current = english;

void strings_init(void)
{
    if (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE)
        current = portuguese;
}

const wchar_t *tr(enum str_id id)
{
    return current[id];
}
