# CLAUDE.md

Orientações para agentes trabalhando neste repositório.

## O que é

Utilitário Win32 em C que filtra o clique duplo fantasma de mouses com defeito
por meio de um hook `WH_MOUSE_LL`. É uma reimplementação open-source do antigo
MouseFix (2004); a análise do original está em `docs/analise-do-original.md`.

## Estrutura e responsabilidades

- `src/debounce.c/.h`: toda a lógica de decisão (bloquear, segurar, injetar).
  **Não pode incluir `windows.h`** nem chamar o sistema: recebe eventos e
  devolve ações em `struct db_result`. Qualquer mudança de comportamento do
  filtro começa aqui, com teste em `tests/test_debounce.c`.
- `src/main.c`: hook, ícone da bandeja, menu, timers e `SendInput`. Só traduz
  eventos para o filtro e executa o que ele devolve.
- `src/settings.c/.h`: leitura/gravação em `HKCU\Software\MouseFix` e entrada
  `MouseFix` em `HKCU\...\CurrentVersion\Run`.
- `res/`: ícones, manifesto (`asInvoker`, DPI) e `VERSIONINFO`. A versão
  aparece em `CMakeLists.txt`, `res/mousefix.rc` e `res/mousefix.manifest`;
  mude as três juntas. `mousefix.ico` é o ícone do `.exe` e da bandeja (tem
  versões simplificadas de 16 a 32 px); `docs/icone.png` é a cópia de 128 px
  usada no README.

## Comandos

Testes do filtro (funcionam em Linux/macOS/Windows):

```bash
cc -std=c99 -Wall -Wextra -pedantic -Isrc src/debounce.c tests/test_debounce.c -o /tmp/test_debounce && /tmp/test_debounce
```

Build no Windows:

```bash
cmake -B build && cmake --build build --config Release && ctest --test-dir build -C Release
```

Compilação cruzada a partir do Linux sem MinGW instalado, com Zig
(`pip install ziglang`); troque `x86_64` por `x86` para 32 bits:

```bash
python -m ziglang cc -target x86_64-windows-gnu -O2 -s -Wall -Wextra -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -Isrc src/main.c src/settings.c src/debounce.c res/mousefix.rc -o MouseFix.exe -Wl,--subsystem,windows -luser32 -lshell32 -ladvapi32
```

Teste de ponta a ponta sem Windows: Wine dentro de um X aninhado (Xephyr), um
programa auxiliar que registra `WM_LBUTTONDOWN/UP` e `xdotool` enviando
sequências como `mousedown 1 sleep 0.08 mouseup 1 sleep 0.01 mousedown 1`.
Use sempre um display separado para não mover o mouse real do usuário. Esses
scripts não fazem parte do repositório.

## Publicar uma versão

Atualize a versão nos três arquivos acima e crie uma tag `vX.Y.Z`. A
workflow `.github/workflows/build.yml` compila x86 e x64, roda os testes e,
em tags, publica uma release com **um único `MouseFix.exe` (32 bits)**, que
roda em qualquer Windows, com o texto de `.github/release-notes.md` e o
SHA-256. Não publicar mais de um `.exe`: o público é leigo e não deve
precisar escolher.

## Regras do projeto

- Zero dependências além da API do Windows; um único `.exe`, sem DLL.
- Nunca pedir administrador nem acessar rede ou arquivos.
- Eventos com `LLMHF_INJECTED` nunca são filtrados.
- O hook precisa ser rápido (o Windows remove hooks lentos): nada de E/S,
  alocação ou chamadas bloqueantes dentro de `mouse_hook`.
- Ao pausar, trocar configuração ou sair, chamar `release_pending()` para não
  deixar um botão "preso" no modo seguro para arrastar.
- Experiência para leigos: sem janela, então toda abertura manual mostra uma
  notificação dizendo onde está o ícone; a primeira execução pergunta sobre o
  início automático; abrir uma segunda vez avisa a instância existente em vez
  de mostrar erro. Início automático passa `/autostart` e fica silencioso.
- Interface, comentários e documentação em português do Brasil. Fontes em
  UTF-8 (o CMake passa `/utf-8` ao MSVC); o `.rc` deve permanecer ASCII.
- C99, estilo do código existente: 4 espaços, chaves de função na linha
  seguinte, prefixo `db_` no filtro e `g_` em globais.

## Fora do repositório

O script que gera os ícones (Python + Pillow) e os scripts de teste no Wine
ficam em `../MouseFix-extras/`, fora do repositório.

Os binários originais (`MouseFix.exe`, `MouseFixDll.dll`) e os dumps de
desmontagem ficam fora desta pasta e **nunca** devem ser adicionados ao
repositório (licença desconhecida).
