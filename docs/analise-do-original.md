# Análise do MouseFix original

Engenharia reversa do `MouseFix.exe` + `MouseFixDll.dll` que circula na
internet como correção para clique duplo. O objetivo foi entender exatamente
o que ele faz e verificar se havia algo além disso. Nenhum código ou binário
do original faz parte deste repositório.

## Arquivos analisados

| Arquivo           | Tamanho  | SHA-256                                                            |
| ----------------- | -------- | ------------------------------------------------------------------ |
| `MouseFix.exe`    | 40 960 B | `98bb12182d9e61918fd96813c1644e29a6125c86ee43ee28c43acac86de14efa` |
| `MouseFixDll.dll` | 45 056 B | `cd168c8b04f1c637834c975370bc780f93676fab0698e62be07e1e30446904bc` |

Ambos são PE32 (x86, 32 bits) de 29/01/2004, compilados com Visual C++ 6 e
CRT estática. Não têm assinatura digital nem ASLR/DEP
(`DllCharacteristics = 0`). Se os seus arquivos tiverem outro hash, esta
análise não se aplica a eles.

Ferramentas: `objdump` (cabeçalhos, importações, exportações e desmontagem).
Quase todo o código dos dois arquivos é a CRT do Visual C++; o código próprio
do programa ocupa cerca de 0x2F0 bytes no `.exe` e 0x60 bytes na DLL.

## MouseFixDll.dll

Exporta duas funções e tem uma seção compartilhada `.SHARDAT` de 8 bytes
(duas variáveis visíveis a todos os processos que carregarem a DLL).

A única função de sistema que a DLL importa, fora da CRT, é
`USER32!CallNextHookEx`.

Reconstrução a partir da desmontagem (`0x10001010` e `0x10001020`):

```c
#pragma data_seg(".SHARDAT")
HHOOK g_hook     = 0;   /* 0x1000a000 */
DWORD g_lastUpMs = 0;   /* 0x1000a004 */
#pragma data_seg()

BOOL SetHook(HHOOK h)                       /* exportada como "SetHook" */
{
    g_hook = h;
    return TRUE;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{                                           /* "_LowLevelMouseProc@12" */
    if (nCode == HC_ACTION) {
        DWORD t = ((MSLLHOOKSTRUCT *)lParam)->time;
        if (wParam == WM_LBUTTONDOWN) {
            if (t - g_lastUpMs < 40)        /* cmp eax,0x28 ; jae */
                return 1;                   /* descarta o clique */
        } else if (wParam == WM_LBUTTONUP) {
            g_lastUpMs = t;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}
```

## MouseFix.exe

O programa só carrega a DLL, instala o hook e espera:

1. `RegisterClassA("MouseFixClass")` e `CreateWindowExA` de uma janela que é
   imediatamente escondida (`ShowWindow(SW_HIDE)`).
2. Em `WM_CREATE`: `LoadLibraryA("MouseFixDll.dll")`, `GetProcAddress` de
   `_LowLevelMouseProc@12` e `SetHook`, depois
   `SetWindowsHookExA(WH_MOUSE_LL, proc, hDll, 0)` e `SetHook(hHook)`.
   Se algo falhar, mostra uma `MessageBox` e fecha.
3. Laço de mensagens. A tecla Esc com a janela em foco (que nunca está
   visível) envia `WM_CLOSE`.
4. Ao sair: `UnhookWindowsHookEx`, `SetHook(NULL)`, `FreeLibrary`.

Importações relevantes: `LoadLibraryA`, `GetProcAddress`, `FreeLibrary`,
`SetWindowsHookExA`, `UnhookWindowsHookEx` e funções de janela/mensagem de
`USER32` e `GDI32`.

## Avaliação de segurança

**Nada malicioso foi encontrado.** Não há importações nem código de rede,
registro, arquivos (exceto a saída de erro da CRT), processos, injeção de
código ou persistência.

Pontos fracos, ainda que não maliciosos:

- **Carregamento da DLL por nome relativo.** `LoadLibraryA("MouseFixDll.dll")`
  segue a ordem de busca de DLLs do Windows, então outra DLL com esse nome
  numa pasta pesquisada antes seria carregada no lugar (*DLL hijacking*).
- **Binários sem assinatura, de 2004, sem ASLR/DEP**, distribuídos por sites
  de terceiros. Não há como saber se a cópia baixada é a original sem
  comparar o hash.
- **DLL desnecessária.** Hooks `WH_MOUSE_LL` não são injetados em outros
  processos; o Windows chama a função no processo que instalou o hook. A DLL
  com seção compartilhada é um padrão de hooks globais (`WH_MOUSE`), que aqui
  não acrescenta nada.

## Comportamento e limitações do original

- Filtra apenas o botão esquerdo, com intervalo fixo de 40 ms.
- Quando descarta um "apertar", deixa passar o "soltar" seguinte. Os programas
  recebem um "soltar" sem "apertar" correspondente.
- Se o defeito acontece durante um arraste (soltar–apertar rápido no meio do
  movimento), o "soltar" fantasma passa e o arraste é interrompido.
- Sem ícone, sem janela visível e sem forma de fechar além do Gerenciador de
  Tarefas. Sem proteção contra duas instâncias.

## O que esta versão mantém e muda

| Aspecto               | Original                    | Esta versão                                    |
| --------------------- | --------------------------- | ---------------------------------------------- |
| Critério de bloqueio  | "apertar" < 40 ms após "soltar" | Igual no modo padrão                       |
| "soltar" órfão        | Passa                       | Descartado junto com o "apertar"               |
| Arraste interrompido  | Não trata                   | Modo "seguro para arrastar"                    |
| Botões                | Esquerdo                    | Configurável (5 botões)                        |
| Intervalo             | 40 ms fixo                  | Configurável                                   |
| Eventos injetados     | Também filtrados            | Nunca filtrados                                |
| Estrutura             | `.exe` + DLL                | Um único `.exe`                                |
| Interface             | Nenhuma                     | Ícone na bandeja com menu                      |
