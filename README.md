## Bootloader, прошивка firmware

### Общие сведения

В новых версиях наших устройств реализован механизм загрузчика прошивок
(bootloader). Таким образом, код делится на две части: сам загрузчик и
основная программа, код которой содержится в прошивке.

Bootloader реализует следующий функционал:

  - запуск основной программы;
  - обновление прошивки устройства по интерфейсу RS-485, протокол Modbus
    RTU;
  - сброс настроек UART и Modbus-адреса для основной программы в
    значения по умолчанию;
  - сброс всех настроек устройства в значения по умолчанию.

~~[удалено]~~

Коммуникационные параметры (скорость, четность, количество стоп-битов и
битов данных, сигнатура устройства) сохраняются в EEPROM-чипе. **В
режиме bootloader'а коммуникационные параметры фиксированы и не
зависят от значений в EEPROM: 9600 8N2.** Выбор Modbus-адреса
устройства описан ниже.

### Переход в режим bootloader'а

Устройство переходит в режим bootloader'а:

  - если основная программа присутствует; при каждом включении питания
    bootloader активен в течение 2 секунд;
  - если основная программа отсутсвует в памяти (например, был сбой при
    обновлении прошивки); в этом случае bootloader активен постоянно;
  - При работе основной программы в holding-регистр 129 (0x81) была
    записана 1. В этом случае устанавливается соответствующий флаг,
    устройство перезагружается и остается в режиме bootloader'а 120
    секунд;
  - ~~[удалено]~~

~~[удалено]~~

Индикация режима bootloader'а: индикатор Status редко мигает. При
заливке прошивки индикатор перестает менять состояние.

### Прошивки

Прошивка основной программы выполняется bootloader'ом, который получает
ее по Modbus RTU. Прошивка состоит из 32-байтного информационного блока
(сигнатура устройства, размер прошивки) и 136-байтных блоков данных.
~~[удалено]~~

Прошивки хранятся в файлах с расширением **.wbfw**.

### Сигнатуры устройств

При первой записи прошивки на заводе в EEPROM устройства записывается
сигнатура — наименование аппаратного типа устройства. При перезаписи
прошивки текущая сигнатура сравнивается с сигнатурой из информационного
блока прошивки. Запись новой прошивки возможна только при совпадении
сигнатур.

~~[удалено]~~

### Modbus

Прошивка основной программы выполняется bootloader'ом, который получает
ее по Modbus RTU. В режиме bootloader'а коммуникационные параметры
фиксированы и не зависят от значений в EEPROM: 9600 8N2. По Modbus
устройство можно перевести в режим bootloader'а, если во время работы
основной программы записать 1 в holding-регистр 129 (0x81). В этом
случае устанавливается соответствующий флаг, устройство
перезагружается и остается в режиме bootloader'а 120
секунд.

Modbus-адрес для прошивки в режиме bootloader'а следует выбирать
следующим образом:

1.  Основной адрес для прошивки — адрес, заданный в EEPROM, то есть это
    стандартный адрес, на который устройство отвечает из основной
    программы.
2.  Прошивать устройства можно по широковещательному адресу 0. Это менее
    предпочтительный способ при рекомендации его конечным пользователям.

3.  ~~[удалено]~~

В режиме bootloader поддерживается ограниченный набор Modbus-команд.
Фактически мы можем записывать данные в holding-регистры командами
Write Single Register (0x06) и Write Multiple Registers (0x10). **Адреса
регистров и их назначение не обсуждаем в публичном пространстве.**

#### Этапы прошивки:

* Команда `BEGIN`: (запись `16` регистров начиная с адреса `0x1000` командой `0x10`)
* Команда `DATA`: (запись `68` регистров начиная с адреса `0x2000` командой `0x10`)


####  Сброс настроек UART и Modbus-адреса:

* выполняется путем записи ненулевого значения в регистр `1000` (`0x03E8`) командой `0x06`;
* сбрасывает настройки UART и Modbus-адреса в заводские (1, 9600N2)
* при успешном выполнении посылает стандартный ответ Modbus.
* ~~[удалено]~~

#### Сброс всех настроек устройства:

* выполняется путем записи ненулевого значения в регистр `1001 (0x03E9)` командой 0x06;
* ~~[удалено]~~
* сбрасывает вообще все настройки устройства, хранящиеся в области памяти с настройками в EEPROM.
* при успешном выполнении посылает стандартный ответ Modbus.

Из основной программы доступны следующие holding-регистры.

#### Переход в режим загрузчика:
holding-регистр `0x81` (`129`) -- записать ненулевое значение.


#### Сигнатура устройства
Доступна для чтения через input-регистры `290-301`.

#### Версия bootloader'а
input-регистры `330-337`

### EEPROM

~~[удалено]~~

### Утилита для прошивки firmware

Заливка прошивок с контроллера или компьютера с Linux выполняется
утилитой wb-mcu-fw-flasher (этот репозиторий)
Установка пакета

```
  apt-get update
  apt-get install wb-mcu-fw-flasher
```

Опции запуска:

```
  root@wirenboard-AEYANPGT:/etc/apt/sources.list.d# wb-mcu-fw-flasher
  Welcome to Wiren Board flash tool.
  Usage:
  Param  Description                                               Default value
   -d     Communication device                                      -
   -f     Firmware file                                             -
   -a     Modbus ID                                                 1
   -j     Send jump to bootloader command                           -
   -u     Reset UART setting and MODBUS address to factory default  -
   -e     Format EEPROM (except device signature)                   -
   -r     Jump to bootloader register address                       129
   -D     Debug mode                                                -
  Minimal example: ./flasher -d /dev/ttyUSB0 -f firmware.wbfw
```

Опция -j позволяет прошиать устройство при его работе в основной
программе.

### Прошивка firmware

## Прошивка прошивки

При прошивке с контроллера остановить wb-mqtt-serial.

~~[удалено]~~. Modbus-адрес
устройства после прошивки загрузчика -- 1.

Подключите устройство к RS-485.

Если прошивка была записана ранее, то сигнатуру устройства можно
прочесть командой

```
    export mbusaddr=1;  echo  -e `modbus_client --debug -mrtu -pnone -s2 /dev/ttyRS485-1 -a$mbusaddr -t0x03 -r290 -c 12 | grep Data | sed -e 's/0x00/\x/g' -e 's/Data://' -e 's/s//g'`|  xxd -r -p && echo ''

```

Получите сигнатуру устройства, например, **wbmr6c**

Выберите подходящую прошивку, напрмер,
WB-MR-MR6C\_MCU3\_3\_ULN2003\_1.9.4\_feature-bootloader\_1.9.3\_5932761.wbfw

Прошейте устройство командой:

```
   wb-mcu-fw-flasher -j -d /dev/ttyRS485-1 -a 1 -f WB-MR-MR6C_MCU3_3_ULN2003_1.9.4_feature-bootloader_1.9.3_5932761.wbfw
```

Успешный процесс прошивки выглядит следующим образом:

```
   wb-mcu-fw-flasher -j -d /dev/ttyRS485-1 -a 1 -f WB-MR-MR6C_MCU3_3_ULN2003_1.9.4_feature-bootloader_1.9.3_5932761.wbfw
   /dev/ttyRS485-1 opened successfully.
   Send jump to bootloader command and wait 2 seconds...
   Error: Connection timed out.
   May be device already in bootloader, try to send firmware...
   WB-MR-MR6C_MCU3_3_ULN2003_1.9.4_feature-bootloader_1.9.3_5932761.wbfw opened successfully, size 12136 bytes
   Sending info block... OK
   Sending data block 89 of 89... OK.
   All done!
```

После успешной прошивки устройство перезапустится в основной программе.

## Прошивка устройств по широковещательному Modbus-адресу 0

Bootloader позволяет загружать прошивку на устройства но Modbus-адресу
0. Для этого устройство должно быть единственным устройством на шине и
находится в режиме загрузчика.

## Обновление бутлоадера

~~[удалено]~~

## Прошивка нескольких устройств на шине

Возможна прошивка нескольких устройств, подключенных к шине. Устройства
должны быть переведены в режим bootloader и прошиваться по отдельности
с указанием нешироковещательного Modbus-адреса. Прошивка устройств с
одинаковым адресом возможна, если в режиме bootloader находится
только одно из них. Устройства прошиваются по очереди.

**Внимание\!** Прошивка устройств, находящимся в режиме bootloader и
имеющим одинаковый modbus-адрес, или же прошивка нескольких
устройств, находящихся в режиме bootloader по адресу 0 не будет
выполнена; имеющаяся прошивка на устройствах будет испорчена.

## Быстрая прошивка для производства

В загрузчике с версии 1.1.4 появилась возможность загружать прошивку
самый первый раз на скорости 115200 для этого необходимо
использовать wb-mcu-fw-flasher от версии 1.0.5 с флагом -B
115200 (ключ в верхнем регистре не имеет ничего общего с -b и задает
скорость отправки частей прошивки загрузчику. не описан в хэлпе
прогаммы так как пользователям не нужно его использовать). Режим
приема прошивки на скорости 115200 активен когда нет сигнатуры -
пустая еепром с завода или когда ~~[удалено]~~.

~~[удалено]~~

## Карта Modbus регистров загрузчика

| Регистр | Длина | Тип     | Команда        | Назначение                          | Примечание     |
| ------- | ----- | ------- | -------------- | ----------------------------------- | -------------- |
| 1000    | 1     | holding | single write   | Сброс настроек связи                |                |
| 1001    | 1     | holding | single write   | Сброс всех настроек кроме сигнатуры |                |
| ~~[удалено]~~    | 1     | holding | single write   | ~~[удалено]~~             |~~[удалено]~~ |
| 0x1000  | 16    | holding | multiple write | Пакет с заголовком                  |                |
| 0x2000  | 68    | holding | multiple write | Пакет с телом загрузчика            |                |
| 290     | 12    | holding | read           | Чтение сигнатуры устройства         | С версии 1.1.7 |
| 330     | 8     | holding | read           | Чтение версии загрузчика            | С версии 1.1.7 |

## Внутреннее использование загрузчика

~~[удалено]~~
