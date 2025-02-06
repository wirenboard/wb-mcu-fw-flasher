## Загрузчик (bootloader)
### Общие сведения

Во всех наших устройствах реализован механизм загрузчика прошивок (bootloader). Таким образом, код делится на две части: сам загрузчик и основная программа, код которой содержится в прошивке.

Загрузчик реализует следующий функционал:

  - запуск основной программы
  - обновление прошивки устройства по интерфейсу RS-485, протокол Modbus RTU
  - сброс Modbus-настроек для основной программы на значения по умолчанию
  - сброс всех настроек и калибровок устройства на значения по умолчанию.

**В режиме загрузчика коммуникационные параметры Modbus фиксированы и не зависят
от настроек на устройстве: 9600 8N2.** Начиная с версии загрузчика 1.3.0 имеется возможность перейти из прошивки в загрузчик с сохранением коммуникационных параметров.

### Переход в режим загрузчика

Устройство переходит в режим загрузчика:
  - если основная программа присутствует - при каждом включении питания загрузчик активен в течение 2 секунд
  - если основная программа отсутсвует в памяти (например, был сбой при обновлении прошивки) - в этом случае загручик активен постоянно
  - при работе основной программы в holding-регистр 129 (0x81) была записана 1. В этом случае устанавливается соответствующий флаг, устройство перезагружается и остается в режиме загрузчика 120 секунд. Коммуникационные параметры Modbus устанавливаются в значения по умолчанию (9600 8N2), адрес не меняется
  - при работе основной программы в holding-регистр 131 (0x83) была записана 1. В этом случае устанавливается соответствующий флаг, устройство перезагружается и остается в режиме загрузчика 120 секунд. Параметры связи при этом не меняются (остаются такими же, какими были в основной программе). **Поддерживается начиная с версии загрузчика 1.3.0.** Также необходима поддержка со стороны прошивки (подробнее - в changelog прошивки).

В режиме загрузчика индикатор Status мигает 1 раз в 2 секунды.

### Сигнатуры устройств

При первой записи прошивки на заводе в устройство записывается сигнатура — наименование аппаратного типа устройства. При перезаписи прошивки текущая сигнатура сравнивается с сигнатурой из информационного блока прошивки. Запись новой прошивки возможна только при совпадении сигнатур.

### Modbus-адрес

Прошивка основной программы выполняется загрузчиком, который получает ее по Modbus RTU.

Modbus-адрес для загрузки прошивки следует выбирать следующим образом:

1.  Основной адрес для прошивки — адрес, хранящийся в памяти устройства (EEPROM или FlashFS в зависимости от устройства), то есть это стандартный адрес, на который устройство отвечает из основной программы.
2.  Прошивать устройства можно по широковещательному адресу 0. Это менее предпочтительный способ при рекомендации его пользователям.

В режиме загрузчика поддерживается ограниченный набор Modbus-команд. Фактически мы можем записывать данные в holding-регистры командами Write Single Register (0x06) и Write Multiple Registers (0x10). **Адреса регистров и их назначение не обсуждаем в публичном пространстве.**

### Сброс Modbus-настроек

* выполняется путем записи ненулевого значения в регистр `1000` (`0x03E8`) командой `0x06`
* Modbus-настройки (адрес и коммуникационные параметры) сбрасываются на заводские (1, 9600N2)
* при успешном выполнении посылает стандартный ответ Modbus
* *Modbus-адрес загрузчика после выполнения команды остаётся прежним*. После загрузки прошивки или перезагрузки по питанию Modbus-адрес установится на 1

### Сброс настроек и калибровок устройства, хранящихся в EEPROM (при наличии)

*Данное действие затрагивает только устройства, у которых есть EEPROM.*
* выполняется путем записи ненулевого значения в регистр `1001 (0x03E9)` командой `0x06`
* если у устройства есть заводские калибровки (например, MS, MSW), они будут сброшены. После этого устройство будет неверно измерять значения
* для старых прошивок (подробнее - в changelog прошивки) сбрасывает также настройки устройства, хранящиеся в EEPROM
* сбрасывает Modbus-настройки на заводские (1, 9600N2)
* при успешном выполнении посылает стандартный ответ Modbus
* не рекомендуется для самостоятельного применения. Если вы не уверены в том, что делаете - обратитесь в техподдержку
* *Modbus-адрес загрузчика после выполнения команды остаётся прежним*. После загрузки прошивки или перезагрузки по питанию Modbus-адрес установится на 1

### Сброс настроек устройства, хранящихся во FlashFS

*Данное действие не затрагивает Modbus-настройки (даже если они хранятся во FlashFS).*
* поддерживается версией загрузчика `1.2.0` и выше
* выполняется путем записи ненулевого значения в регистр `1002 (0x03EA)` командой `0x06`
* должна быть поддержка со стороны прошивки (подробнее - в changelog прошивки)
* безопасно для использования: калибровки не потеряются, а если нет поддержки со стороны прошивки - ничего не будет выполнено
* при успешном выполнении посылает стандартный ответ Modbus

### Переход в режим загрузчика

* holding-регистр `0x81` (`129`) -- записать 1 - загрузчик будет работать с параметрами 9600N2.
* holding-регистр `0x83` (`131`) -- записать 1 - загрузчик будет работать с текущими настройками соединения. Нужна поддержка со стороны прошивки и загрузчик версии 1.3.0 и выше. Подробнее - в changelog прошивки. Возможны следущие варианты поведения:
  - если запись прошла успешно - устройство перезагрузилось в загрузчик на текущих параметрах соединения, можно продолжать процесс обновления прошивки
  - если вернулась ошибка Illegal Data Address - устройство не поддерживает данную функцию, нужно обновить прошивку и/или загрузчик

### Сигнатура устройства

Доступна для чтения через holding-регистры `290-301` командой `0x03`.
Пример команды:

```bash
echo -e $(modbus_client -mrtu -pnone -s2 /dev/ttyRS485-1 -a1 -t0x03 -r290 -c12 | grep Data | sed -e 's/.*Data://' -e 's/ 0x00/\\x/g')
```

### Версия загрузчика

Доступна для чтения через holding-регистры `330-337` командой `0x03`.

```bash
echo -e $(modbus_client -mrtu -pnone -s2 /dev/ttyRS485-1 -a1 -t0x03 -r330 -c8 | grep Data | sed -e 's/.*Data://' -e 's/ 0x00/\\x/g')
```

## Обновление загрузчика и прошивки
### Этапы прошивки

* Команда `BEGIN`: (запись `16` регистров начиная с адреса `0x1000` командой `0x10`)
* Команда `DATA`: (запись `68` регистров начиная с адреса `0x2000` командой `0x10`)

### Формат прошивки

Прошивка основной программы выполняется загрузчиком, который получает ее по Modbus RTU. Прошивка состоит из 32-байтного информационного блока (сигнатура устройства, размер прошивки) и 136-байтных блоков данных.
Прошивки хранятся в файлах с расширением **.wbfw**.

### Утилита для обновления

Загрузка прошивок с контроллера или ПК с Linux выполняется утилитой wb-mcu-fw-flasher (этот репозиторий).
Установка пакета:

```bash
  apt-get update
  apt-get install wb-mcu-fw-flasher
```

Опции запуска:

```
Welcome to Wiren Board flash tool.
Version: 1.4.0, libmodbus version: 3.1.7

Usage:

Param  Description                                         Default value

-d     Serial port (e.g. "/dev/ttyRS485-1")                      -
-s     Stopbits (2/1)                                   auto: (2sb->, ->1sb)
-f     Firmware file                                             -
-a     Modbus ID (slave addr)                                    1
-j     Jump to bootloader using reg 129                          -
         uses 9600N2 for communicate with bootloader (can be changed with -B key)
-J     Jump to bootloader using reg 131                          -
         uses baudrate from -b key to communicate with bootloader (don`t use -B)
-u     Reset UART setting and MODBUS address to factory default  -
-w     Reset device settings stored in flash to factory default  -
-D     Debug mode                                                -
-b     Baudrate used to communicate with firmware                9600
-B     Baudrate used to communicate with bootloader              9600
-p     Parity                                                    N
-t     Slave response timeout (in seconds)                       10.0

Examples:

Flashing device that is in bootloader:
    wb-mcu-fw-flasher -d <port> -f <firmware.wbfw>
    useful for flashing device immediately after power on

Reset uart settings:
    wb-mcu-fw-flasher -d <port> -a10 -u

Flashing running device:
    wb-mcu-fw-flasher -d <port> -a <modbus_addr> -j -f <firmware.wbfw>

Flashing running device using custom baudrate:
    wb-mcu-fw-flasher -d <port> -a <modbus_addr> -b115200 -J -f <firmware.wbfw>
    useful for flashing device behind Modbus-TCP gateway

Only read device info (no flashing):
    wb-mcu-fw-flasher -d <port> -a10 --get-device-info

```

Опция -j позволяет прошивать устройство при его работе в основной
программе.

### Процесс загрузки прошивки

При прошивке с контроллера остановить wb-mqtt-serial.
Подключить устройство к RS-485.
Если прошивка была записана ранее, то сигнатуру устройства можно прочесть командой

```bash
export mbusaddr=1;  echo  -e `modbus_client --debug -mrtu -pnone -s2 /dev/ttyRS485-1 -a$mbusaddr -t0x03 -r290 -c 12 | grep Data | sed -e 's/0x00/\x/g' -e 's/Data://' -e 's/s//g'`|  xxd -r -p && echo ''
```

Получите сигнатуру устройства, например, **mr6cG**
Выберите подходящую прошивку, например, mr6cG__1.17.7_master_52bdb26.wbfw

Прошейте устройство командой:

```bash
wb-mcu-fw-flasher -j -d /dev/ttyRS485-1 -a 1 -f mr6cG__1.17.7_master_52bdb26.wbfw
```
Успешный процесс прошивки выглядит следующим образом:

```
  wb-mcu-fw-flasher -j -d /dev/ttyRS485-1 -a 1 -f mr6cG__1.17.7_master_52bdb26.wbfw
  /dev/ttyRS485-2 opened successfully.
  Send jump to bootloader command and wait 2 seconds...
  Ok, device will jump to bootloader.
  mr6cG__1.17.7_master_52bdb26.wbfw opened successfully, size 24648 bytes

  Sending info block... OK

  Sending data block 181 of 181... OK.

  All done!
```

После успешной прошивки устройство перезапустится в основную программу.

### Прошивка устройств по широковещательному Modbus-адресу 0

Загрузчик позволяет загружать прошивку на устройства по Modbus-адресу
0. Для этого устройство должно быть единственным устройством на шине и
находиться в режиме загрузчика.

### Обновление загрузчика

Обновление загрузчика происходит так же, как и обновление прошивки - загрузкой файла `.wbfw` в устройство.

**ВНИМАНИЕ:** обновление загрузчика — потенциально опасная операция. Если во время обновления пропало питание, устройство может превратиться в «кирпич». В этом случае гарантия на него аннулируется. Чтобы избежать такой ситуации — резервируйте питание контроллера и устройства, например, с помощью WB-UPS v2.

### Прошивка нескольких устройств на шине

Возможна прошивка нескольких устройств, подключенных к шине. Устройства должны быть переведены в режим загрузчика и прошиваться по отдельности с указанием нешироковещательного Modbus-адреса. Прошивка устройств с одинаковым адресом возможна, если в режиме загрузчика находится только одно из них. Устройства прошиваются по очереди.

**Внимание\!** Прошивка устройств, находящихся в режиме загрузчика и имеющих одинаковый Modbus-адрес, или же прошивка нескольких устройств, находящихся в режиме загрузчика, по адресу 0 не будет выполнена - имеющаяся прошивка на устройствах будет испорчена.

### Прошивка устройств на заданной скорости

Начиная с версии загрузчика `1.3.0` и версии wb-mcu-fw-flasher `1.3.0` поддерживается переход из прошивки в загрузчик с сохранением параметров соединения (скорость, чётность).
Для этого необходимо использовать опцию `-J` в wb-mcu-fw-flasher.
Также необходима поддержка в прошивке (см. changelog прошивки конкретного устройства).

При использовании флага `-J` общение с загрузчиком продолжается на той же скорости, на которой отправляется команда перехода в загрузчик (задается флагом `-b`).

Если устройство было переведено в загрузчик через 131 регистр вручную или прошлая попытка обновления с флагом `-J` была неудачной, то нужно использовать флаг `-B` для задания скорости обновления прошивки без флагов `-j` и `-J`.

### Быстрая прошивка для производства

В загрузчике с версии 1.1.4 появилась возможность загружать прошивку самый первый раз на скорости 115200 для этого необходимо использовать wb-mcu-fw-flasher версии 1.0.5 и выше с флагом -B
115200. Ключ в верхнем регистре не имеет ничего общего с -b и задает скорость отправки частей прошивки загрузчику, не описан в справке программы, так как пользователям не нужно его использовать. Режим приема прошивки на скорости 115200 также активен когда нет сигнатуры - пустая память с завода.

## Карта Modbus регистров загрузчика

| Регистр | Длина | Тип     | Команда        | Назначение                                            | Примечание     |
| ------- | ----- | ------- | -------------- | ----------------------------------------------------- | -------------- |
| 1000    | 1     | holding | single write   | Сброс Modbus-настроек                                  |                |
| 1001    | 1     | holding | single write   | Сброс всех настроек и калибровок, хранящихся в EEPROM |                |
| 1002    | 1     | holding | single write   | Сброс всех настроек, хранящихся во FlashFS            | С версии 1.2.0 |
| 1003    | 1     | holding | read           | Максимальное количество блоков данных новой прошивки, которое не приведёт к потере настроек, хранящихся во FlashFS, при обновлении прошивки  | С версии 1.2.0 |
| 1004    | 1     | holding |  single write  | Переход в прошивку  | С версии 1.4.0 |
| 0x1000  | 16    | holding | multiple write | Пакет с заголовком                                    |                |
| 0x2000  | 68    | holding | multiple write | Пакет с телом загрузчика                              |                |
| 290     | 12    | holding | read           | Чтение сигнатуры устройства                           | С версии 1.1.7 |
| 330     | 8     | holding | read           | Чтение версии загрузчика                              | С версии 1.1.7 |
