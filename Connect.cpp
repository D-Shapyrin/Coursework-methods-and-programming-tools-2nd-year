/**
 * @authentication_file Connect.cpp
 */
#include "Connect.h"
#include "md5.h"

// Заливка текста при выводе в консоль: https://stackoverflow.com/a/21786287/21356333 
string CSI = "\x1B[";

/**
 * @brief Получение из файла логина и пароля.
 */

void Connect::GetLoginPassword(){
    // Открытие файла для аутентификации
    
    ifstream authentication_file(name_auto_file);
    if(!authentication_file.is_open()){
        authentication_file.close();
        throw error_proj(std::string("fun:GetLoginPassword, param:name_auto_file.\n" + 
                                    CSI+"31;40m" + "Ошибка отрытия файла для аутентификации! Содержание \'name_auto_file\': " + CSI+"0m" + name_auto_file));
    }

    string line;
    if(authentication_file.peek() == EOF){
        authentication_file.close();
        throw error_proj(std::string("fun:GetLoginPassword, param:name_auto_file.\n"+
                                    CSI+"31;40m"+"Ошибка отрытия файла для аутентификации! Файл пуст!"+CSI+"0m"));
    }

    getline(authentication_file, line);              // Считывание строки из файла
    int spaceIndex = line.find(" ");  // Поиск индекса первого пробела
    string usernamePart = line.substr(0, spaceIndex);  // Извлечение имени пользователя до пробела
    string passwordPart = line.erase(0, spaceIndex + 1);  // Извлечение пароля после пробела
    username = usernamePart;          // Присвоение имяени пользователя
    password = passwordPart;          // Присвоение пароля


}

/**
* @brief Взаимодействие с сервером.
* @param ip_addr адрес сервера.
* @param port порт сервера.
* @throw error_proj при возникновении ошибки.
*/


int Connect::Connect_to_server(const char *ip_addr, int port)
{
    GetLoginPassword();

    // Открытие файла для чтения векторов
    ifstream input_file(name_original_file);
    if(!input_file.is_open()){
        input_file.close();
        throw error_proj(std::string("fun:Connect_to_server, param:name_original_file.\n"+
                                    CSI+"31;40m"+"Ошибка отрытия файла с векторами!"+CSI+"0m"));
    }

    string line;
    if(input_file.peek() == EOF){
        input_file.close();
        throw error_proj(std::string("fun:Connect_to_server, param:name_original_file.\n"+
                                    CSI+"31;40m"+"Ошибка отрытия файла. Файл с векторами пуст!"+CSI+"0m"));
    }

    // Открытие файла для записи суммы
    ofstream output_file(name_result_file, ios::binary);
    if (!output_file.is_open()){
        output_file.close();
        throw error_proj(std::string("fun:Connect_to_server, param:name_result_file.\n"+
                                    CSI+"31;40m"+"Файл для результата не может быть открыт или создан!"+CSI+"0m"));
    }

    try{
        ip_addr;
    }
    catch (...){ // Catch absolute everything that was thrown and drop it.
        throw error_proj(std::string("fun:Connect_to_server, param:ip_addr.\n"+
                                    CSI+"31;40m"+"Не возможно получить адрес!"+CSI+"0m"));
    }

    try{
        port;
    }
    catch (...){ // Catch absolute everything that was thrown and drop it.
        throw error_proj(std::string("fun:Connect_to_server, param:port.\n"+
                                    CSI+"31;40m"+"Не возможно получить порт!"+CSI+"0m"));
    }
    
    char buffer[1024];
    int bytes_read;

    int socket_fd; // fd - file descriptor (файловый дескриптор)
    struct sockaddr_in addr;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        throw error_proj("Function: Connect_to_server, Parameter: socket_fd.\n" + 
                        CSI + "31;40m" + "Ошибка создания сокета!" + CSI + "0m");
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    if(connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        throw error_proj(std::string("fun:Connect_to_server, param:socket_fd.\n"+
                                    CSI+"31;40m"+"Ошибка подключения к серверу!"+CSI+"0m"));
    }

    // Отправка логина клиента
    strcpy(buffer, username.c_str()); // Копирование username в буфер
    send(socket_fd, buffer, username.length(), 0);

    // Получение строки с SALT или ошибки ERR
    bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0); // Чтение сырых данных из сокета
    string salt(buffer, bytes_read); // Cтрока salt создается на основе данных в buffer, но длина строки ограничена значением bytes_read => строка будет содержать только те байты которые реально были считаны bytes_read => "\0" не требуется.
    if (salt == "ERR"){
        close(socket_fd);
        throw error_proj(std::string("fun:Connect_to_server, param:username.\n"+
                                    CSI+"31;40m"+"Ошибка идентификации!"+CSI+"0m"));
    }

    // Вычисление хэша-кода от SALT+PASSWORD
    string combinedSaltPassword = salt + password;
    string hashedPassword = MD5_hash(combinedSaltPassword);

    // Отправка хэша-кода от SALT+PASSWORD
    strcpy(buffer, hashedPassword.c_str()); // Копирование hashedPassword в буфер
    send(socket_fd, buffer, hashedPassword.length(), 0);

    // Получене ответа об аутентификации
    bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0); // Чтение сырых данных из сокета
    string isAuthenticated(buffer, bytes_read); // Cтрока isAuthenticated создается на основе данных в buffer, но длина строки ограничена значением bytes_read => строка будет содержать только те байты которые реально были считаны bytes_read => "\0" не требуется.
                                                // Удачно "OK", ошибка "ERR"
    if (isAuthenticated == "ERR"){
        close(socket_fd);
        throw error_proj(std::string("fun:Connect_to_server, param:password.\n"+
                                    CSI+"31;40m"+"Ошибка аутентификации!"+CSI+"0m"));
    }
    
    // Получение количества векторов
    getline(input_file, line);

    int len = 0;
    try{
        len = stoi(line);
    }
    catch (...){
        throw error_proj(std::string("fun:Connect_to_server, param:line\n."+
                                    CSI+"31;40m"+"Не возможно получить количество векторов!"+CSI+"0m"));
    }

    std::cout << "vectorLength: " << len << std::endl;

    output_file.write(reinterpret_cast<char*>(&len), sizeof(len)) << endl;

    // Отправка количества векторов
    send(socket_fd, &len, sizeof(len), 0);

    // Отправка векторов
    for(int l = 0; l < len; l++){
        getline(input_file, line);

        // Получение длины вектора и его значений
        string vectorString = line;
        int index = 0;
        int spacePos = vectorString.find(" ");
        string lengthString = vectorString.substr(0, spacePos);
        vectorString = vectorString.erase(0, spacePos+1);
        int vectorLength = 0;
        try{
            vectorLength = stoi(lengthString);
        }
        catch (...){
            throw error_proj(std::string("fun:Connect_to_server, param:line.\n"+
                                        CSI+"31;40m"+"Не возможно получить размер вектора!"+CSI+"0m"));
        }
        
        uint32_t vectorArray[vectorLength] = {0};

        for(int index = 0; index < vectorLength; index++){
            string elementString;
            int i = vectorString.find(" ");
            elementString = vectorString.substr(0, i);
            vectorString = vectorString.erase(0, i+1);

            try{
                vectorArray[index] = stof(elementString);
            }
            catch (...){
                throw error_proj(std::string("fun:Connect_to_server, param:vectorString-строка.\n"+
                                            CSI+"31;40m"+"Не возможно получить вектор!"+CSI+"0m"));
            }

        }

        send(socket_fd, &vectorLength, sizeof(vectorLength), 0);
        send(socket_fd, &vectorArray, sizeof(vectorArray), 0);
        uint32_t sum = 0;
        recv(socket_fd, &sum, sizeof(sum), 0); // Получение суммы от сервера
        
        std::cout << "Sum(" << 1+l << "): " << sum << std::endl;
        
        // Так как write() работает с байтами, с помощью reinterpret_cast указатель на переменную len (int) преобразуеися в указатель на тип char*. sizeof говорит сколько байт нужно запистаь в файл. 
        output_file.write(reinterpret_cast<char*>(&sum), sizeof(sum)) << endl;
    }

    // Закрытие файлов
    input_file.close();
    output_file.close();

    // Сохранение результатов в файле
    cout << "Результат сохранен в файле: " << name_result_file << endl;
    close(socket_fd);

    return 0;
}
