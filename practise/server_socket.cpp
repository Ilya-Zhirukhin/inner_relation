#include <iostream>
#include <vector>
#include <mutex>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <fstream>

using WebSocketServer = websocketpp::server<websocketpp::config::asio>;
using MessagePtr = WebSocketServer::message_ptr;

std::mutex dataMutex;
std::vector<int16_t> data{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Функция сериализации массива в бинарные данные
std::vector<uint8_t> serializeData(const std::vector<int16_t>& data) {
    std::vector<uint8_t> serializedData(data.size() * sizeof(int16_t));
    std::memcpy(serializedData.data(), data.data(), serializedData.size());
    return serializedData;
}

// Функция десериализации бинарных данных в массив
std::vector<int16_t> deserializeData(const std::vector<uint8_t>& serializedData) {
    std::vector<int16_t> result(serializedData.size() / sizeof(int16_t));
    std::memcpy(result.data(), serializedData.data(), serializedData.size());
    return result;
}

void onClientConnect(WebSocketServer* server, websocketpp::connection_hdl hdl) {
    try {
        std::lock_guard<std::mutex> lock(dataMutex);

        // Загрузка данных из файла
        std::ifstream file("data.txt", std::ios::binary);
        if (file) {
            std::vector<char> fileData(std::istreambuf_iterator<char>(file), {});
            if (fileData.size() == data.size() * sizeof(int16_t)) {
                data = std::vector<int16_t>(reinterpret_cast<int16_t*>(fileData.data()), reinterpret_cast<int16_t*>(fileData.data() + fileData.size()));
            } else {
                std::cout << "Invalid data size in file" << std::endl;
            }
            file.close();
        } else {
            std::cout << "Error loading data from file" << std::endl;
        }

        std::vector<uint8_t> initialData = serializeData(data);
        initialData.insert(initialData.begin(), 0); // Добавляем тип сообщения для инициализации данных
        server->send(hdl, initialData.data(), initialData.size(), websocketpp::frame::opcode::binary);
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending initial data: " << e.message() << std::endl;
    }
}

void onMessageReceived(WebSocketServer* server, websocketpp::connection_hdl hdl, MessagePtr message) {
    std::vector<uint8_t> receivedData(message->get_payload().begin(), message->get_payload().end());

    if (receivedData.size() >= sizeof(int16_t)) {
        std::vector<int16_t> updatedData;
        updatedData.reserve(receivedData.size() / sizeof(int16_t));

        for (size_t i = 0; i < receivedData.size(); i += sizeof(int16_t)) {
            int16_t value;
            std::memcpy(&value, receivedData.data() + i, sizeof(int16_t));
            updatedData.push_back(value);
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            data = updatedData;
        }

        // Вывод сообщения об изменении массива данных
        std::cout << "Данные были изменены:" << std::endl;
        for (const auto& value : data) {
            std::cout << value << " ";
        }
        std::cout << std::endl;

        // Сохранение данных в файл
        std::ofstream file("data.txt", std::ofstream::binary | std::ofstream::trunc);
        if (file) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(int16_t));
            file.close();
        } else {
            std::cout << "Error saving data to file" << std::endl;
        }

        std::vector<uint8_t> updatedDataBytes = serializeData(data);
        updatedDataBytes.insert(updatedDataBytes.begin(), 1); // Добавляем тип сообщения перед данными
        server->send(hdl, updatedDataBytes.data(), updatedDataBytes.size(), websocketpp::frame::opcode::binary);
    }
}


void saveDataToFile() {
    std::lock_guard<std::mutex> lock(dataMutex);
    std::ofstream file("data.txt", std::ios::binary);
    if (file) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(int16_t));
        file.close();
    } else {
        std::cout << "Error saving data to file" << std::endl;
    }
}

void onClientClose(WebSocketServer* server, websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(dataMutex);

    // Сохранение данных в файл
    std::ofstream file("data.txt", std::ofstream::binary | std::ofstream::trunc);
    if (file) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(int16_t));
        file.close();
    } else {
        std::cout << "Error saving data to file" << std::endl;
    }
}


int main() {
    WebSocketServer server;

    server.set_reuse_addr(true);
    server.set_listen_backlog(500);

    server.set_open_handler([&server](websocketpp::connection_hdl hdl) {
        onClientConnect(&server, hdl);
    });



    server.set_message_handler([&server](websocketpp::connection_hdl hdl, MessagePtr message) {
        onMessageReceived(&server, hdl, message);
    });

    server.set_close_handler([&server](websocketpp::connection_hdl hdl) {
        onClientClose(&server, hdl);
    });

    server.init_asio();
    server.listen(22345);
    server.start_accept();

    std::cout << "Server is running on port 22345" << std::endl;

    // Запускаем отдельный поток для сохранения данных в файл с определенной периодичностью
    std::thread saveThread([](){
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Задержка в 10 секунд
            saveDataToFile();
        }
    });

    server.run();
    saveThread.join(); // Дожидаемся завершения потока сохранения данных

    return 0;
}