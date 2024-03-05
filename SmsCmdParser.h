//-----------------------------------------------------------------------------------------------------
//  Класс для парсинга СМС команд типа <command_name><spases...><data1><spases...><data2>... Пример: "Min2 10.5"
//  Результат парсинга не зависит от количества пробелов после команды и между блоками данных
//-----------------------------------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

//Результаты парсинга
enum paeseResult {
  PARSE_INVALID_DATA  = -2,   // -2 - Данные не соответствуют заданным условиям
  PARSE_CMD_NOT_FOUND,        // -1 - Команда не найдена
  PARSE_NOT_ENOUGH_DATA,      // 0 - Не достаточно данных пришло
  PARSE_OK                    // 1 - ОК
};

class SmsCmdParser {
public:
  //Подключаем буфер с текстом  
  SmsCmdParser( const char* data ) {
    pdata = data;
  }
  ~SmsCmdParser() {
    clear();
  }
    
  //Освободить буфер - вызывать после окончания парсинга, либо сам вызовется в деструкторе при выходе из цикла
  void clear() {
    for ( int i = 0; i < qtyBlock; i++ )   //Освободить внешний буфер
      if ( pstr[i] ) free(pstr[i]);
    if (pstr) { 
      free(pstr);   //Освободить буфер указателей
      pstr = 0;
    }
    qtyBlock = 0; 
  }

  //Парсинг - принимает имя команды и ожидаемое количество блоков данных после этой команды.
  //Возвращает: -1 - команда не найдена, 
  //            0 - команда найдена, но недостаточно блоков данных, 
  //            1 - команда найдена, блоков данных достаточно 
  int parsing ( const char* cmd, int amData = 0 ) {
    int i = 0, j = 0, amount = 0; 
    size_t szBlock;
    int state = 0;
    char* p2block = 0;
    bool f_endPars = 0; //флаг завршения парсинга на этом блоке

    char *p = findCmd(cmd);   //Ищем команду
    if (p == 0 )      return  PARSE_CMD_NOT_FOUND;      //Не найдено 
    if (amData == 0)  return  PARSE_OK;    //Если не задано количество, то считаем что успешно 
      
    if (pstr) clear();  //Освободить память если идет повторный парсинг
    pstr = (char**)calloc(amData, sizeof(char*));   // создаём буфер указателей на наши подстроки

    p += strlen(cmd); // следующий после заголовка 
    //Парсим по одному символу
    while ( 1 ) 
    {
      //если валидные символы еще не начались
      switch (state) {
        case 0:    //Данные еще не начались
          while (p[i] == ' ')  i++; //Пропустить все пробелы перед данными

          p2block = p + i; // запоминаем текущий указатель на блок данных 
          
          // проверка на конец строки и неформат
          if ( !isValid( p[i] ) ) { 
            f_endPars = 1;
            state = 3; // заканчиваем, данных не найдено
            break;
          }
          // теперь идут валидные данные
          amount++;       //Считаем блоки данных
          i++;
          state = 1;
          break;
        
        case 1: //Далее перебираем данные 
          // если попался пробел, значит блок нормально завершен
          if ( p[i] == ' ' ) {     
            state = 2; 
            break;
          } else
          // Если конец строки и неформат
          if ( !isValid( p[i] ) ) {     // закончилась строка - это конец парсинга
            f_endPars = 1;
            state = 2; 
            break;
          }
          i++; 
          break;

        case 2: //Завершение блока
          szBlock = (p+i - p2block) / sizeof(char);           // вычисляем размер блока
          if (szBlock)  //Если данных не найдено, то не копируем ничего
            pstr[j++] = cpyToExtBuf( p2block, szBlock ); //Копируем и сохраняем ук. на внешний буфер

          qtyBlock = amount;  //Сохраняем количество блоков данных

          if (f_endPars)  //Если пришел конец строки 
            if ( amount < amData )  return  PARSE_NOT_ENOUGH_DATA;   //Данных недостаточно
            else                    return  PARSE_OK;
          else 
            if ( amount < amData ) {  //Данные еще не все 
              i++;
              state = 0; //В начало
              break; 
            }
            else  return PARSE_OK;  //Данные собраны

        default:  
          return PARSE_NOT_ENOUGH_DATA;
      }
    } 
  }

  //Найти команду (ищет как с верхним так и с нижним регистром первого символа) 
  char* findCmd(const char* cmd)
  {
    char* ptr = strstr(pdata, cmd); //Ищем заголовок
    if (ptr == 0) {     //Если такого не нашлось 
      char cmd_[ strlen(cmd) + 1 ];     //Делаем копию заголовка для non static преобразования    
      strcpy(cmd_, cmd);

      //Если верхний регистр первой буквы был
      if      (cmd_[0] >= 'A'  &&  cmd_[0] <= 'Z' )  cmd_[0] += 0x20; //Переводим в нижний регистр 
      else if (cmd_[0] >= 'a'  &&  cmd_[0] <= 'z')   cmd_[0] -= 0x20; //Переводим в верхний регистр 
      ptr = strstr(pdata, cmd_);   //Повторяем поиск
    }
    return ptr; //  0 если не нашелся 
  }

  //Копировать блок данных из входной строки по указателю pToData во внешний буфер extBuf
  //количеством qty
  char* cpyToExtBuf( char* pToData, int qty ) {
      int i = 0;
      
      if (qty == 0) return 0;  //Если размер нулевой, ночего не копировать

      extBuf = (char*)calloc( qty + sizeof(char), sizeof(char) );      // создаём буфер под этот размер       
      while (1) {
          if (i >= qty) break;
          extBuf[i++] = *pToData++;
      }
      extBuf[i] = '\0';
      return extBuf;
  }

  //Проверка символа на конец строки и неформат
  int isValid (char ch) {
    if (ch == '\0'  ||  ch == '\n'  ||  ch == '\r') 
      return 0;
    return 1;
  }

  //Получить количечство найденных блоков данных 
  int amount () {
      return qtyBlock;
  }

  // получить инт из выбранной подстроки
  int32_t getInt(int num) {
      return atol(pstr[num]);
  }
  
  // получить float из выбранной подстроки
  float getFloat(int num) {
      return atof(pstr[num]);
  }
  
  // сравнить подстроку с другой строкой
  bool equals(int num, const char* comp) {
      return !strcmp(pstr[num], comp);
  }

  //Парсер для float переменных
  int checkFloat(int num, float *pval, float minLim = NAN, float maxLim = NAN) {
    float val = getFloat(num); //Промежуточная переменная
    
    if (!isnan(minLim)) //Если указано
      if (val < minLim)  //Проверка на соответствие
        return 0; //Проверка не пройдена
        
    if (!isnan(maxLim)) //Если указано
      if (val > maxLim)  //Проверка на соответствие
        return 0; //Проверка не пройдена
    
    *pval = val; //Присвоить внешней переменной
    return 1; //Проверка пройдена
  }

  //Парсер для целочисленных переменных
  int checkInt(int num, int* pval, int minLim, int maxLim) {
    int val =  getInt(num); //Промежуточная переменная

    if (val < minLim)  //Проверка на соответствие
      return 0; //Проверка не пройдена

    if (val > maxLim)  //Проверка на соответствие
      return 0; //Проверка не пройдена
    
    *pval = val; //Присвоить внешней переменной
    return 1; 
  }

  //--------------------------------------------------------------------------------
  //Полный цикл парсера команд типа [cmd][nom][spase][val], пример: "tmin1 12.5"
  //Проверяет соответствие данных установленным тут же пределам 
  //--------------------------------------------------------------------------------
  int parse_cmd_nom_pval (const char* cmd, float* pval, size_t sizeVal,
                          float VAL_MIN = NAN, float VAL_MAX = NAN,
                          int NOM_MIN = 0, int NOM_MAX = 0xFFFF  
                          ) 
  {
    int res = parsing(cmd, 2); //Парсим команду
    if (res == PARSE_OK ) //Найдены все параметры 
    {
      int nom = 0; 
      if (checkInt(0, &nom, NOM_MIN, NOM_MAX)) { //Проверяем номер
        if (nom >= sizeVal/sizeof(pval[0]))  //Если индекс перевалил за количество членов массива
          return PARSE_INVALID_DATA; 
        if (checkFloat(1, &pval[nom], VAL_MIN, VAL_MAX))  //Если успешно, парсим переменную                      
          return PARSE_OK;
      } 
      return PARSE_INVALID_DATA; //Данные не прошли проверку, введено не верно
    }
    return res; //Возвратить остальные ошибки
  }

  //--------------------------------------------------------------------------------
  //Полный цикл парсера команд типа [cmd][spase][val], вариант для float, пример: "tmin 12.5"
  //Проверяет соответствие данных установленным тут же пределам 
  //--------------------------------------------------------------------------------
  int parse_cmd_pval (const char* cmd, float* pval, 
                    float VAL_MIN = NAN, float VAL_MAX = NAN) 
  {
    int res = parsing(cmd, 1); //Парсим команду
    if (res == PARSE_OK ) //Найдены все параметры 
    {
      if (checkFloat(0, pval, VAL_MIN, VAL_MAX))  //Если успешно, парсим переменную                      
          return PARSE_OK;
      return PARSE_INVALID_DATA; //Данные не прошли проверку, введено не верно
    }
    return res; //Возвратить остальные ошибки
  }

  //--------------------------------------------------------------------------------
  //Команда без аргументов, просто ищет команду
  //--------------------------------------------------------------------------------
  int parse_cmd (const char* cmd) {
    return parsing(cmd);
  }

  //--------------------------------------------------------------------------------
  //Полный цикл парсера команд типа [cmd][spase][val], вариант для int, пример: "tmin 2" 
  //Проверяет соответствие данных установленным тут же пределам 
  //--------------------------------------------------------------------------------
  int parse_cmd_pval (const char* cmd, int* pval, 
                    float VAL_MIN = NAN, float VAL_MAX = NAN) 
  {
    int res = parsing(cmd, 1); //Парсим команду
    if (res == PARSE_OK ) //Найдены все параметры 
    {
      if (checkInt(0, pval, VAL_MIN, VAL_MAX))  //Если успешно, парсим переменную                      
          return PARSE_OK;
      return PARSE_INVALID_DATA; //Данные не прошли проверку, введено не верно
    }
    return res; //Возвратить остальные ошибки
  }

  //Парсер для текстовых полей (пример: "Garage")
  void cpyText(int num, char* buf, int maxLen = 64) {
    int i = 0; char ch;
    while (1) {
      if (i >= maxLen) break;
      ch = pstr[num][i]; 
      if (!ch) break; //Пока не придет конец строки
      buf[i++] = ch;  //Копировать в буфер
    }
    buf[i] = '\0';
  }

  const char* pdata = NULL; // указатель на входную строку
  char* extBuf = NULL;  //Указатель на буфер для хранения блоков данных
  char** pstr = NULL; //Массив указателей на начала блоков данных во входной строке. 
  
  char* operator [] (uint16_t idx) {
      return pstr[idx];
  }  

private:
  int qtyBlock = 0;    // количество блоков данных в принятом пакете 
};
