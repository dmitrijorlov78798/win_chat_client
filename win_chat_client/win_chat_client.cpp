﻿
#include <iostream>
#include <string>
#include <list>
#include <chrono>

#include "network.h"

#ifdef __WIN32__
#include <conio.h>
#else 
#include <unistd.h>
#endif

#define IP_ADRES "127.0.0.1"


/// <summary>
/// тип сообщений
/// </summary>
enum TypeMsg
{
    defaul, // неопознанное
    normal, // нормальное, с полезной нагрузкой
    // сервисные:
    Exit, // отключение клиента
    shutDown, // отключение сервера
    linkOn, // собеседники на связи
    printinfo // вывод информации по соединению
};

/// <summary>
/// класс декоратор над std::string для хранения сообщения, состоящего из 
/// заголовка (тип сообщения)-6 символов + текст + конец сообщения(EOM)-5 символов
/// </summary>
class msg_t
{
public:
    /// <summary>
    /// контсруктор
    /// </summary>
    /// <param name="type"> -- тип сообщения</param>
    /// <param name="text"> -- текст сообщения</param>
    msg_t(TypeMsg type = TypeMsg::defaul, std::string text = "") : offset(0)
    {
        switch (type)
        {
        case normal:
            this->text = "[NORM]" + text + "[EOM]"; // полезная нагрузка лишь здесь
            break;
        case Exit:
            this->text = "[EXIT][EOM]";
            break;
        case shutDown:
            this->text = "[SHUT][EOM]";
            break;
        case linkOn:
            this->text = "[LINK][EOM]";
            break;
        case printinfo:
            this->text = "[INFO][EOM]"; 
            break;
        default:
            break;
        }
    }

    /// <summary>
    /// метод получения не константной ссылки на внутренний std::string с сообщением
    /// </summary>
    /// <returns> не константная ссылка на внутренний std::string с сообщением </returns>
    std::string& Update()
    {
        offset = 0;
        return text;
    }

    /// <summary>
    /// метод получения константной ссылки на внутренний std::string с сообщением 
    /// </summary>
    /// <returns> константная ссылка на внутренний std::string с сообщением </returns>
    const std::string& Str() const
    {
        return text;
    }

    /// <summary>
    /// метод получения типа сообщения
    /// </summary>
    /// <returns> типа сообщения </returns>
    TypeMsg Type() const
    {
        TypeMsg result = TypeMsg::defaul;

        if (text.size() >= 6)
        {
            std::string header(text, 0, 6); // извлекаем заголовок
            if (header == "[NORM]")
                result = TypeMsg::normal;
            else if (header == "[EXIT]")
                result = TypeMsg::Exit;
            else if (header == "[SHUT]")
                result = TypeMsg::shutDown;
            else if (header == "[LINK]")
                result = TypeMsg::linkOn;
            else if (header == "[INFO]")
                result = TypeMsg::printinfo;
        }

        return result;
    }

    /// <summary>
    /// метод получения конца сообщения
    /// </summary>
    /// <returns> конец сообщения </returns>
    std::string EOM() const
    {
        return "[EOM]";
    }

    /// <summary>
    /// метод получения текста сообщения без заголовка и конца сообщения
    /// </summary>
    /// <param name="buf"> -- ссылка на буфер, куда будет положен полезный текст сообщения </param>
    ///  <returns> 1 -- текст получен </returns>
    bool Print(std::string& buf) const
    {
        buf.clear();
        switch (Type())
        { // расшифровка сервесных сообщений
        case TypeMsg::shutDown:
            buf = "SYSTEM MSG: server get command shutdown";
            break;
        case TypeMsg::Exit:
            buf = "SYSTEM MSG: server get command exit from visavi client";
            break;
        case TypeMsg::printinfo:
            buf = "SYSTEM MSG: other visavi not connected";
            break;
        case TypeMsg::linkOn:
            buf = "SYSTEM MSG: server get connected from other visavi";
            break;
        case TypeMsg::defaul:
            buf = "SYSTEM MSG: server recived defined message";
            break;
        case TypeMsg::normal:
            buf.assign(text, 6, text.size() - 11); // выдаем текст без заголовка и конца сообщения
            break;
        default:
            break;
        }
        
        return !text.empty();
    }

    /// <summary>
    /// метод задания смещения
    /// </summary>
    /// <param name="offset"> -- смещение </param>
    void SetOffset(unsigned offset)
    {
        if (offset < text.size())
            this->offset = offset;
    }

    /// <summary>
    /// метод возврата смещения
    /// </summary>
    /// <returns> -- смещение </returns>
    unsigned GetOffset() const
    {
        return offset;
    }

protected:
    std::string text; // строка хранящее сообщение, согласно формату, опраделенному выше
    unsigned offset; // смещение от начала сообщения
};

/// <summary>
/// класс наблюдатель за соединением
/// </summary>
class info_t
{
protected:
    /// <summary>
    /// метод получения секунд с 1970 года
    /// </summary>
    /// <returns> секунды с 1970 года </returns>
    long long GetTimeSec() const
    {
        auto now = std::chrono::system_clock::now().time_since_epoch(); // 1970 год
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now); // секунды
        return sec.count();
    }
public:
    /// <summary>
    /// конструктор
    /// </summary>
    info_t() : b_connectedVisavi(false), b_connectedServer(false), byte(0), sec(0)
    {}

    /// <summary>
    /// метод мониторинга связи с собеседником
    /// </summary>
    /// <param name="flag"> -- флаг связи</param>
    void ConnectedVisavi(bool flag)
    {
        flag &= b_connectedServer; // маскируем признак по наличию связи с сервером
        if (b_connectedVisavi && !flag)
        { // если связь пропала
            byte = 0;
            sec = 0;
        }
        // если связь появилась
        if (!b_connectedVisavi && flag)
            sec = GetTimeSec(); // задаем время ее появления

        b_connectedVisavi = flag;
    }

    /// <summary>
    ///  метод мониторинга связи с сервером
    /// </summary>
    /// <param name="flag"> -- флаг связи </param>
    void ConnectedServer(bool flag)
    {   // если связь пропала с сервером
        if (b_connectedServer && !flag)
            ConnectedVisavi(flag); // то пропала и с собеседником

        b_connectedServer = flag;
    }

    /// <summary>
    /// метод подсчета трафика в байтах
    /// </summary>
    /// <param name="size"> -- размер переданного/принятого сообщения </param>
    void AddCountByte(unsigned size)
    {
        if (b_connectedVisavi) // считаем трафик только с визави
            byte += size;
    }

    /// <summary>
    /// метод возврата состояния связи с собеседником
    /// </summary>
    /// <returns> 1 -- связь есть</returns>
    bool ConnectedVisavi() const
    {
        return b_connectedVisavi;
    }

    /// <summary>
    /// метод возврата состояния связи с сервером
    /// </summary>
    /// <returns> 1 -- связь есть</returns>
    bool ConnectedServer() const
    {
        return b_connectedServer;
    }

    /// <summary>
    /// метод возврата трафика в байтах с момента подключения
    /// </summary>
    /// <returns> N - количество байт в трафике </returns>
    unsigned Byte() const
    {
        return byte;
    }

    /// <summary>
    /// метод возврата времени в секундах с момента подключения
    /// </summary>
    /// <returns> N - количество секунд </returns>
    unsigned Sec() const
    {
        long long result = 0;
        if (sec) // если было подключение
            result = GetTimeSec() - sec; // вычисляем разность
        return result;
    }

protected:
    bool b_connectedVisavi; // флаг состояния связи с собеседником
    bool b_connectedServer; // флаг состояния связи с сервером
    unsigned byte; // количество байт в трафике с момента подключения
    long long sec; // количество секунд с момента подключения
};

#ifdef __WIN32__
/// <summary>
/// класс реализующий взаимодействие с пользователем через консоль
/// </summary>
class console_t
{
public:
    /// <summary>
    /// конструктор
    /// </summary>
    console_t()
    {
#else
/// <summary>
/// класс реализующий взаимодействие с пользователем через консоль
/// </summary>
class console_t : public network::socket_t // если линукс, можем сделать консоль - неблокирующим файловым дескриптором (сокетом)
{
public:
    /// <summary>
    /// конструктор
    /// </summary>
    console_t(log_t& logger) : network::socket_t(logger)
    {
        Socket = 0; // дескриптор stdin
#endif
        std::cout << "command :\nINFO -- print info client connection\nSHUTDOWN -- close server\nEXIT -- close this client\n";
    }

    /// <summary>
    /// метод парсинга ввода с консоли
    /// </summary>
    /// <param name="msgBuf"> -- ссылка на список буферных сообщений </param>
    /// <returns> 1 -- добавлено валидное сообщение </returns>
    bool ParseInput(std::list<msg_t>& msgBuf)
    {
        bool result = false;

#ifdef __WIN32__
        buf.clear();
        if (_kbhit()) // для винды определяет нажатие клавишы
        {
            std::getline(std::cin, buf);
            result = true;
        }
#else
        buf.resize(512, '\0');
        unsigned size = read(Socket, &buf[0], buf.size());
        if (size > 0) 
        {   // избавляемся от лишнего в буфере
            std::string tmp(buf, 0, size - 1); 
            buf.swap(tmp);
            result = true;
        }
#endif
        if (result) // что то прочитали?
        {   // парсим команды
            if (buf == "EXIT")
                msgBuf.push_back(msg_t(TypeMsg::Exit));
            else if (buf == "SHUTDOWN")
                msgBuf.push_back(msg_t(TypeMsg::shutDown));
            else if (buf == "INFO")
                msgBuf.push_back(msg_t(TypeMsg::printinfo));
            else
                msgBuf.push_back(msg_t(TypeMsg::normal, "VISAVI MSG: " + buf));
        }

        return result;
    }

    /// <summary>
    /// метод вывода сообщения на экран
    /// </summary>
    /// <param name="msgBuf"> ссылка на сообщение для вывода </param>
    void PrintMsg(const msg_t msgBuf) const
    {
        std::string out;
        if (msgBuf.Print(out))
            std::cout << out << '\n';
    }

    /// <summary>
    /// метод вывода диагностической информации
    /// </summary>
    /// <param name="info"> -- ссылка на класс наблюдатель за соединением </param>
    void PrintInfo(const info_t& info)
    {
        std::cout << "info: connected visavi: " << info.ConnectedVisavi()
            << " connected server: " << info.ConnectedServer()
            << " time: " << info.Sec() << "sec byte: " << info.Byte() << '\n';
    }
protected:
    std::string buf; // буферная строка
};

/// <summary>
/// класс управления чатом
/// </summary>
class chat_manager_t
{
public:
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="port"> -- номер порта для подключения </param>
    chat_manager_t(unsigned port) : logger(), multiplexor(logger), u_counter(0), b_exit(false), b_shut(false), b_echo(false)
    {
        socket = std::make_shared<network::TCP_socketClient_t>(IP_ADRES, port, logger);
        multiplexor.AddReader(socket);
#ifndef __WIN32__
        console = std::make_shared<console_t>(logger);
        multiplexor.AddReader(console);
#endif
    }

    // деструктор
    ~chat_manager_t()
    {
        if (socket->GetConnected() && !b_exit) // отключаем свое соединение на сервере
        {
            msg_t tmp(TypeMsg::Exit);
            socket->Send(tmp.Str());
        }
    }
    /// <summary>
    /// основной метод работы
    /// </summary>
    void Work()
    {
        while (!b_exit)
        {
            multiplexor.Work(50);
            // отправка
#ifdef __WIN32__
            if (console.ParseInput(l_msg_TX) || !l_msg_TX.empty())
#else
            if (multiplexor.GetReadyReader(console) && console->ParseInput(l_msg_TX) || !l_msg_TX.empty())
#endif
            {
                //std::cout << "OUT: " << l_msg_TX.front().Str() << '\n'; ////////////////////////////////наладка
                for (auto it = l_msg_TX.begin(); it != l_msg_TX.end(); ) // идем по списку сообщений
                    if (it->Type() == TypeMsg::printinfo)
                    {
#ifdef __WIN32__
                        console.PrintInfo(info); // вывод информации
#else
                        console->PrintInfo(info); // вывод информации
#endif
                        it = l_msg_TX.erase(it);
                    }
                    else
                    {
                        if (it->Type() == TypeMsg::Exit) // мониторим команду на выход
                        {
                            b_exit = true;
                            if (!socket->GetConnected())
                            {
                                it = l_msg_TX.erase(it);
                                continue;
                            }
                        }
                        if (it->Type() == TypeMsg::shutDown) // мониторим команду на отключение сервера
                            b_shut = true;

                        int code = socket->Send(it->Str(), it->GetOffset()); // отправляем сообщение
                        if (0 == code) // если сообщение отправлено полностью
                        {
                            info.AddCountByte(it->Str().size()); // считаем трафик
                            it = l_msg_TX.erase(it); // удаляем сообщение
                        }
                        else if (0 < code) // если отправили часть
                        {
                            it->SetOffset(code); // запоминаем где остановились
                            break; // до следующей итерации
                        }
                        else if (-3 == code) // сокет не готов к отправке
                            break; // до следующей итерации
                        else if (-1 == code)// системная ошибка
                            it = l_msg_TX.erase(it); // удаляем сообщения
                        else if (-2 == code) // сокет закрыт
                        {
                            it = l_msg_TX.erase(it);
#ifdef __WIN32__
                            console.PrintMsg(msg_t(TypeMsg::normal, "SYSTEM MSG: server not connected")); // диагностируем
#else 
                            console->PrintMsg(msg_t(TypeMsg::normal, "SYSTEM MSG: server not connected")); // диагностируем
#endif
                        }
                    }
            }
            // прием
            if (multiplexor.GetReadyReader(socket) && 0 == socket->Recive(msg_RX.Update(), msg_RX.EOM())) // если пришли данные и сообщение полное
            {
                //std::cout << "IN: " << msg_RX.Str() << '\n'; ////////////////////////////////наладка
                info.AddCountByte(msg_RX.Str().size()); // считаем трафик

                switch (msg_RX.Type())
                {
                case TypeMsg::linkOn: // подключение собеседника
                    ++u_counter;
                    break;
                case TypeMsg::Exit: // отключение собеседника
                    u_counter = u_counter > 0 ? u_counter - 1 : 0;
                    break;
                case TypeMsg::printinfo: // отсутствует собеседник
                    u_counter = 0;
                    break;
                case TypeMsg::shutDown: // если сервер закрывается, толкаем свои соединения
                    l_msg_TX.push_back(msg_t(TypeMsg::shutDown));
                    b_echo = true;
                    break;
                default:
                    break;
                }
                // вывод сообщения
#ifdef __WIN32__
                console.PrintMsg(msg_RX);
#else
                console->PrintMsg(msg_RX);
#endif
                msg_RX.Update().clear();

                if (b_shut) // сервер отключился по нашей команде
                    socket->ResetConnected();
            }

            if (b_shut && b_echo || b_exit) // сервер отключился по команде другого клиента
                socket->ResetConnected();

            // обновление диагностики
            info.ConnectedServer(socket->GetConnected());
            info.ConnectedVisavi(u_counter > 0);
            b_exit |= !socket->GetConnected() && b_shut;
        }
    }
protected:
    log_t logger;  // объект логгирования
    std::shared_ptr<network::TCP_socketClient_t> socket; // клиентский сокет
#ifdef __WIN32__
    console_t console; // консоль
#else
    std::shared_ptr<console_t> console; // консоль 
#endif
    network::NonBlockSocket_manager_t multiplexor; // мультиплексор неблокирующих сокетов
    msg_t msg_RX; // буфер приходящего сообщения
    std::list<msg_t> l_msg_TX; // буферный список сообщений на отправку
    info_t info; // информация о соединении
    unsigned u_counter; // счетчик собеседников
    bool b_exit; // флаг выхода из программы
    bool b_shut; // флаг отправки команды на отключения сервера
    bool b_echo; // флаг эхоответа на отключение сервера
};

/// <summary>
/// функция разобра параметров командной строки
/// </summary>
/// <param name="argc"> - количество параметров </param>
/// <param name="argv"> - массив параметров </param>
/// <param name="r_port"> - ссылка на порт для прослушки </param>
/// <returns> 1 - праметры распознаны </returns>
bool parseParam(int argc, char* argv[], unsigned& r_port);

int main(int argc, char* argv[])
{
    printf("run_client\n");
    unsigned u32_port = 0;

    if (parseParam(argc, argv, u32_port))
    {
        chat_manager_t chat(u32_port);
        chat.Work();
    }
    else
        printf("Invalid parametr's. Please enter the number_port\n");

    printf("client_shutdown\n");
    return EXIT_SUCCESS;
}

/// <summary>
/// функция разобра параметров командной строки
/// </summary>
/// <param name="argc"> - количество параметров </param>
/// <param name="argv"> - массив параметров </param>
/// <param name="r_port"> - ссылка на порт для прослушки </param>
/// <returns> 1 - праметры распознаны </returns>
bool parseParam(int argc, char* argv[], unsigned& r_port)
{
    bool b_result = false;

    if (argc == 2)
    {
        r_port = std::strtoul(argv[1], NULL, 10);
        b_result = r_port != 0 && r_port != 0xFFFFFFFFUL;
    }

    return b_result;
}

