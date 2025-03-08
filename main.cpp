#include <websocketpp/config/asio_client.hpp> 
#include <websocketpp/client.hpp>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp> 
#include <mutex>
#include <string>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <iomanip> 

// 使用 nlohmann::json 命名空间
using json = nlohmann::json;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client; // 使用 TLS 客户端配置

//填写你的API token
std::string secretKey = "";
std::string apiKey = "";

class Books5{
    public: 
        Books5();
        std::atomic<double> ask1_price;
        std::atomic<double> bid1_price;
        void wait();
    private:
        std::atomic<bool> is_connected; 
        std::atomic<bool> should_reconnect; 
        std::string wss_url;
        client cli;
        void on_message(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg);
        void on_close(client* c, websocketpp::connection_hdl hdl);
        void on_open(client* c, websocketpp::connection_hdl hdl);
        void on_fail(client* c, websocketpp::connection_hdl hdl);
        void run();
        std::thread t;
};

void Books5::wait(){
    // 等待 ASIO 线程结束
    Books5::t.join();
}

Books5::Books5(){
    // 定义并初始化静态成员变量
    Books5::ask1_price.store(0.0, std::memory_order_relaxed);;
    Books5::bid1_price.store(0.0, std::memory_order_relaxed);;
    Books5::is_connected.store(false, std::memory_order_relaxed);; // 标记当前连接状态
    Books5::should_reconnect.store(true, std::memory_order_relaxed);; // 标记是否需要重连
    Books5::wss_url = "wss://ws.okx.com:8443/ws/v5/public";
    Books5::run();
}

void Books5::on_message(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg) {
    try {
        // 解析 JSON 字符串
        json obj = json::parse(msg->get_payload());

        // 获取 arg 对象
        json obj_arg = obj["arg"];

        // 检查 channel 是否为 "books5"
        if (obj_arg["channel"] == "books5") {
            // 获取 data 数组
            json data = obj["data"];

            // 获取 data 数组的第一个元素
            json ab = data[0];

            // 获取 asks 数组
            json asks = ab["asks"];

            // 获取 asks 数组的第一个元素
            json ask1 = asks[0];

            // 解析 ask 价格
            std::string ask_str = ask1[0].get<std::string>();
            double ask_price = std::stod(ask_str);

            // 获取 bids 数组
            json bids = ab["bids"];

            // 获取 bids 数组的第一个元素
            json bid1 = bids[0];

            // 解析 bid 价格
            std::string bid_str = bid1[0].get<std::string>();
            double bid_price = std::stod(bid_str);

            // 更新 Main 类中的静态成员变量
            
            Books5::ask1_price.store(ask_price, std::memory_order_relaxed); // 存储数据
            Books5::bid1_price.store(bid_price, std::memory_order_relaxed); // 存储数据
            /* code */
            //std::cout<<"ask:"<<ask1_price.load(std::memory_order_relaxed)<<"  ";
            //std::cout<<"bid:"<<bid1_price.load(std::memory_order_relaxed)<<std::endl;
        } else {
            // 如果 channel 不是 "books5"，可以在这里处理其他逻辑
            // std::cout << "eee" << std::endl;
        }
    } catch (const std::exception& e) {
        // 捕获并处理 JSON 解析错误
        //std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
}

void Books5::on_close(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "OrderBook Connection closed." << std::endl;
    Books5::is_connected = false;

    if (Books5::should_reconnect) {
        std::cout << "OrderBook Attempting to reconnect..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待 5 秒后重连

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c->get_connection(Books5::wss_url, ec);
        if (ec) {
            std::cout << "OrderBook Reconnect error: " << ec.message() << std::endl;
            return;
        }

        // 设置连接超时时间（例如 5 秒）
        con->set_open_handshake_timeout(2000); // 5 秒

        c->connect(con);
    }
}

void Books5::on_open(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "OrderBook Connection opened." << std::endl;
    Books5::is_connected = true;

    // 每次连接成功后发送一条消息
    websocketpp::lib::error_code ec;
    c->send(hdl, "{\"op\":\"subscribe\",\"args\": [{\"channel\": \"books5\",\"instId\": \"SOL-USDT-SWAP\"}]}", websocketpp::frame::opcode::text,ec);
    if (ec) {
        std::cout << "OrderBook Send failed: " << ec.message() << std::endl;
    } else {
        std::cout << "OrderBook Sent message: OK"<< std::endl;
    }
}

void Books5::on_fail(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "OrderBook Connection failed or timed out." << std::endl;
    Books5::is_connected = false;

    if (Books5::should_reconnect) {
        std::cout << "OrderBook Attempting to reconnect..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待 5 秒后重连

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c->get_connection(Books5::wss_url, ec);
        if (ec) {
            std::cout << "OrderBook Reconnect error: " << ec.message() << std::endl;
            return;
        }

        // 设置连接超时时间（例如 5 秒）
        con->set_open_handshake_timeout(2000); // 5 秒

        c->connect(con);
    }
}


std::string timestampToString() {
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 将时间点转换为 time_t
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

    // 将 time_t 转换为本地时间的 tm 结构体
    struct tm localTime;
    localtime_r(&timestamp, &localTime);

    // 使用 stringstream 格式化时间
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

void Books5::run() {
    

    // 设置日志级别（可选）
    cli.clear_access_channels(websocketpp::log::alevel::all);
    cli.clear_error_channels(websocketpp::log::elevel::all);

    // 初始化 ASIO
    cli.init_asio();

    // 注册消息回调
    cli.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        Books5::on_message(&cli, hdl, msg);
    });

    // 注册连接关闭回调
    cli.set_close_handler([this](websocketpp::connection_hdl hdl) {
        Books5::on_close(&cli, hdl);
    });

    // 注册连接打开回调
    cli.set_open_handler([this](websocketpp::connection_hdl hdl) {
        Books5::on_open(&cli, hdl);
    });

    // 注册连接失败回调
    cli.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        Books5::on_fail(&cli, hdl);
    });

    // 设置 TLS 初始化回调
    cli.set_tls_init_handler([](websocketpp::connection_hdl) {
        // 创建一个新的 TLS 上下文
        return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
    });

    // 创建连接
    websocketpp::lib::error_code ec;
    client::connection_ptr con = cli.get_connection(Books5::wss_url, ec);
    if (ec) {
        std::cout << "OrderBook Connection error: " << ec.message() << std::endl;
        return ;
    }

    // 设置连接超时时间（例如 5 秒）
    con->set_open_handshake_timeout(2000); // 5 秒
    // 连接服务器
    cli.connect(con);

    // 启动 ASIO 事件循环（在单独的线程中运行）
    Books5::t = std::thread([this]() {
        cli.run();
    });

    return ;
}

class Account{
    public: 
        Account();
        void post_order_long(std::string _id,std::string px);
        void post_order_short(std::string _id,std::string px);
        void cancel_order(std::string _id);
        void wait();
        std::string order_id;
        std::atomic<double> order_px;
        std::atomic<double> avgPx;
        std::atomic<double> pos_long;
        std::atomic<double> pos_short;
        std::atomic<double> long_sell_live_px;
        std::atomic<double> short_buy_live_px;
    private:
        std::atomic<bool> is_connected; 
        std::atomic<bool> should_reconnect; 
        
        std::string wss_url;
        client cli;
        void on_message(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg);
        void on_close(client* c, websocketpp::connection_hdl hdl);
        void on_open(client* c, websocketpp::connection_hdl hdl);
        void on_fail(client* c, websocketpp::connection_hdl hdl);
        std::string generateSignature(const std::string& secretKey, const std::string& timestamp);
        std::string getTimestamp();
        std::string base64Encode(const unsigned char* input, int length);
        std::thread t;
        client::connection_ptr con;
        void print(std::string msg);
        
};

void Account::print(std::string msg){
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    
    // 跨平台线程安全处理
    std::tm tm;
    localtime_r(&time, &tm); // Linux/macOS

    std::string a = "buy";
    std::string b = "sell";
    std::string c = "open";
    std::string d = "cancel";
    if(msg.find(c)!=std::string::npos){
        if(msg.find(a)!=std::string::npos){
            std::cout<<"\033[31m"<<std::put_time(&tm, "%Y-%m-%d %H:%M:%S")<<"\t"<<msg<<"\033[0m"<<std::endl;
        }else if(msg.find(b)!=std::string::npos){
            std::cout<<"\033[34m"<<std::put_time(&tm, "%Y-%m-%d %H:%M:%S")<<"\t"<<msg<<"\033[0m"<<std::endl;
        }
    }else if(msg.find(d)!=std::string::npos){
        std::cout<<"\033[32m"<<std::put_time(&tm, "%Y-%m-%d %H:%M:%S")<<"\t"<<msg<<"\033[0m"<<std::endl;
    }
    //std::cout<<std::put_time(&tm, "%Y-%m-%d %H:%M:%S")<<"\t"<<msg<<std::endl;
}
    

void Account::post_order_long(std::string _id,std::string px){
    websocketpp::lib::error_code ec;
    cli.send(Account::con->get_handle(), "{\"id\": \""+_id+"\",\"op\":\"order\",\"args\": [{\"side\": \"buy\",\"posSide\":\"long\",\"instId\": \"SOL-USDT-SWAP\",\"tdMode\": \"isolated\",\"ordType\": \"post_only\",\"clOrdId\":\""+_id+"\",\"sz\": \"0.01\",\"px\":\""+px+"\"}]}", websocketpp::frame::opcode::text,ec);
    if (ec) {
        //std::cout << "post_order Send failed: " << ec.message() << std::endl;
    } else {
        //std::cout << "post order buy: OK"<< std::endl;
        Account::print("open long buy "+px+" "+_id);
    }
}

void Account::post_order_short(std::string _id,std::string px){
    websocketpp::lib::error_code ec;
    cli.send(Account::con->get_handle(), "{\"id\": \""+_id+"\",\"op\":\"order\",\"args\": [{\"side\": \"sell\",\"posSide\":\"short\",\"instId\": \"SOL-USDT-SWAP\",\"tdMode\": \"isolated\",\"ordType\": \"post_only\",\"clOrdId\":\""+_id+"\",\"sz\": \"0.01\",\"px\":\""+px+"\"}]}", websocketpp::frame::opcode::text,ec);
    if (ec) {
        //std::cout << "post_order Send failed: " << ec.message() << std::endl;
    } else {
        //std::cout << "post order buy: OK"<< std::endl;
        Account::print("open short sell "+px+" "+_id);
    }
}

void Account::cancel_order(std::string _id){
    websocketpp::lib::error_code ec;
    auto now = std::chrono::system_clock::now();
    auto ts = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
    cli.send(Account::con->get_handle(), "{\"id\": \""+ts+"\",\"op\":\"cancel-order\",\"args\": [{\"instId\": \"SOL-USDT-SWAP\",\"clOrdId\": \""+_id+"\"}]}", websocketpp::frame::opcode::text,ec);
    if (ec) {
        //std::cout << "cancel_order Send failed: " << ec.message() << std::endl;
    } else {
        //std::cout << "post order buy cancel: OK"<< std::endl;
        Account::print("cancel "+_id);
    }
}

Account::Account(){
    Account::is_connected.store(false, std::memory_order_relaxed); // 标记当前连接状态
    Account::should_reconnect.store(true, std::memory_order_relaxed); // 标记是否需要重连
    Account::pos_long.store(-1.0, std::memory_order_relaxed);
    Account::pos_short.store(-1.0, std::memory_order_relaxed);
    Account::avgPx.store(0.0, std::memory_order_relaxed);
    Account::long_sell_live_px.store(999.9,std::memory_order_relaxed);
    Account::short_buy_live_px.store(0.0,std::memory_order_relaxed);
    Account::order_id="";
    Account::order_px.store(0.0, std::memory_order_relaxed);
    Account::wss_url = "wss://ws.okx.com:8443/ws/v5/private";
        // 设置日志级别（可选）
    cli.clear_access_channels(websocketpp::log::alevel::all);
    cli.clear_error_channels(websocketpp::log::elevel::all);

    // 初始化 ASIO
    cli.init_asio();

    // 注册消息回调
    cli.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        Account::on_message(&cli, hdl, msg);
    });

    // 注册连接关闭回调
    cli.set_close_handler([this](websocketpp::connection_hdl hdl) {
        Account::on_close(&cli, hdl);
    });

    // 注册连接打开回调
    cli.set_open_handler([this](websocketpp::connection_hdl hdl) {
        Account::on_open(&cli, hdl);
    });

    // 注册连接失败回调
    cli.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        Account::on_fail(&cli, hdl);
    });

    // 设置 TLS 初始化回调
    cli.set_tls_init_handler([](websocketpp::connection_hdl) {
        // 创建一个新的 TLS 上下文
        return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
    });

    // 创建连接
    websocketpp::lib::error_code ec;
    Account::con = cli.get_connection(Account::wss_url, ec);
    if (ec) {
        std::cout << "Connection error: " << ec.message() << std::endl;
        return ;
    }

    // 设置连接超时时间（例如 5 秒）
    con->set_open_handshake_timeout(2000); // 5 秒
    // 连接服务器
    cli.connect(con);

    // 启动 ASIO 事件循环（在单独的线程中运行）
    Account::t=std::thread([this]() {
        cli.run();
    });
    
}

void Account::wait(){
    Account::t.join();
}

void Account::on_message(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg) {
    try {
        //std::cout<<msg->get_payload()<<std::endl;
        // 解析 JSON 字符串
        json obj = json::parse(msg->get_payload());
        try{
            json arg = obj["arg"];
            if(arg["channel"]=="positions" && arg["instId"]=="SOL-USDT-SWAP"){
                for (const auto& data : obj["data"]){
                    json posSide = data["posSide"];
                    if(posSide=="long"){
                        std::string pos_str = data["pos"];
                        double pos = std::stod(pos_str);
                        std::string avgPx_str = data["avgPx"];
                        double avgPx = std::stod(avgPx_str);
                        Account::pos_long.store(pos, std::memory_order_relaxed);
                        //Account::avgPx.store(avgPx, std::memory_order_relaxed);
                    }else if(posSide=="short"){
                        std::string pos_str = data["pos"];
                        double pos = std::stod(pos_str);
                        //std::string avgPx_str = data["avgPx"];
                        //double avgPx = std::stod(avgPx_str);
                        Account::pos_short.store(pos, std::memory_order_relaxed);
                    }
                }
                
            }
        }catch(const std::exception& e){}
        try{
            json arg = obj["arg"];
            if(arg["channel"]=="orders" && arg["instId"]=="SOL-USDT-SWAP"){
                
                for (const auto& data : obj["data"]){
                    json posSide = data["posSide"];
                    json side = data["side"];
                    if(posSide=="long" && side=="sell"){
                        std::string state = data["state"];
                        std::string px_str = data["px"];
                        double px = std::stod(px_str);
                        if(state=="live"){
                           
                            Account::long_sell_live_px.store(px, std::memory_order_relaxed);
                            
                        }else if(state=="filled"){
                            if(Account::long_sell_live_px.load(std::memory_order_relaxed)==px){
                                Account::long_sell_live_px.store(999.9, std::memory_order_relaxed);
                            }
                        }
                    }
                    if(posSide=="short" && side=="buy"){
                        std::string state = data["state"];
                        std::string px_str = data["px"];
                        double px = std::stod(px_str);
                        if(state=="live"){
                           
                            Account::short_buy_live_px.store(px, std::memory_order_relaxed);
                            
                        }else if(state=="filled"){
                            if(Account::short_buy_live_px.load(std::memory_order_relaxed)==px){
                                Account::short_buy_live_px.store(0.0, std::memory_order_relaxed);
                            }
                        }
                    }
                    if(posSide=="short" && side=="sell"){
                        std::string fillPx_str = data["fillPx"];
                        double fillPx = std::stod(fillPx_str);
                        std::string fillSz_str = data["fillSz"];
                        std::string clOrdId = data["clOrdId"];
                        std::string state = data["state"];
                        if(fillSz_str!="0" && state=="filled"){
                            //if(clOrdId==Account::order_id){
                            //    Account::order_id="";
                            //    Account::order_px.store(0.0, std::memory_order_relaxed);
                            //}
                            //Account::order_id="";
                            //Account::order_px.store(0.0, std::memory_order_relaxed);
                            //fillPx = fillPx*1.002;
                            fillPx = fillPx*(1-0.0025);
                            Account::short_buy_live_px.store(fillPx, std::memory_order_relaxed);

                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(2) << fillPx; // 保留两位小数
                            fillPx_str = oss.str();
                            
                            auto now = std::chrono::system_clock::now();
                            auto ts = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
                            websocketpp::lib::error_code ec;
                            c->send(hdl, "{\"id\": \""+ts+"\",\"op\":\"order\",\"args\": [{\"side\": \"buy\",\"posSide\":\"short\",\"instId\": \"SOL-USDT-SWAP\",\"tdMode\": \"isolated\",\"ordType\": \"post_only\",\"sz\": \""+fillSz_str+"\",\"px\":\""+fillPx_str+"\"}]}", websocketpp::frame::opcode::text,ec);
                            if (ec) {
                                //std::cout << "post order sell Send failed: " << ec.message() << std::endl;
                            } else {
                                //std::cout << "post order sell: OK"<< std::endl;
                                Account::print("open short buy "+fillPx_str);
                                //Account::order_id="";
                                //Account::order_px.store(0.0, std::memory_order_relaxed);
                            }
                        }
                    }
                    if(posSide=="long" && side=="buy"){
                        std::string fillPx_str = data["fillPx"];
                        double fillPx = std::stod(fillPx_str);
                        std::string fillSz_str = data["fillSz"];
                        std::string clOrdId = data["clOrdId"];
                        std::string state = data["state"];
                        if(fillSz_str!="0" && state=="filled"){
                            //if(clOrdId==Account::order_id){
                            //    Account::order_id="";
                            //    Account::order_px.store(0.0, std::memory_order_relaxed);
                            //}
                            //Account::order_id="";
                            //Account::order_px.store(0.0, std::memory_order_relaxed);
                            //fillPx = fillPx*1.002;
                            fillPx = fillPx*1.0025;
                            Account::long_sell_live_px.store(fillPx, std::memory_order_relaxed);

                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(2) << fillPx; // 保留两位小数
                            fillPx_str = oss.str();
                            
                            auto now = std::chrono::system_clock::now();
                            auto ts = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
                            websocketpp::lib::error_code ec;
                            c->send(hdl, "{\"id\": \""+ts+"\",\"op\":\"order\",\"args\": [{\"side\": \"sell\",\"posSide\":\"long\",\"instId\": \"SOL-USDT-SWAP\",\"tdMode\": \"isolated\",\"ordType\": \"post_only\",\"sz\": \""+fillSz_str+"\",\"px\":\""+fillPx_str+"\"}]}", websocketpp::frame::opcode::text,ec);
                            if (ec) {
                                //std::cout << "post order sell Send failed: " << ec.message() << std::endl;
                            } else {
                                //std::cout << "post order sell: OK"<< std::endl;
                                Account::print("open long sell "+fillPx_str);
                                //Account::order_id="";
                                //Account::order_px.store(0.0, std::memory_order_relaxed);
                            }
                        }
                    }
                }
                
            }
        }catch(const std::exception& e){}
        try{
            std::string event = obj["event"].get<std::string>();
            if(event=="login"){
                // login成功后发送一条消息
                websocketpp::lib::error_code ec;
                c->send(hdl, "{\"op\":\"subscribe\",\"args\": [{\"channel\": \"positions\",\"instType\":\"SWAP\",\"instId\": \"SOL-USDT-SWAP\"}]}", websocketpp::frame::opcode::text,ec);
                if (ec) {
                    //std::cout << "Send failed: " << ec.message() << std::endl;
                } else {
                    std::cout << "subscribe positions: OK"<< std::endl;
                }
                // login成功后发送一条消息
                websocketpp::lib::error_code ec1;
                c->send(hdl, "{\"op\":\"subscribe\",\"args\": [{\"channel\": \"orders\",\"instType\":\"SWAP\",\"instId\": \"SOL-USDT-SWAP\"}]}", websocketpp::frame::opcode::text,ec1);
                if (ec1) {
                    //std::cout << "subscribe orders Send failed: " << ec1.message() << std::endl;
                } else {
                    std::cout << "subscribe orders: OK"<< std::endl;
                }
            }
        }catch(const std::exception& e){

        }
    } catch (const std::exception& e) {
        // 捕获并处理 JSON 解析错误
        //std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
}

void Account::on_close(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "Connection closed." << std::endl;
    Account::is_connected = false;

    if (Account::should_reconnect) {
        std::cout << "Attempting to reconnect..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待 5 秒后重连

        websocketpp::lib::error_code ec;
        Account::con = c->get_connection(Account::wss_url, ec);
        if (ec) {
            std::cout << "Reconnect error: " << ec.message() << std::endl;
            return;
        }

        // 设置连接超时时间（例如 5 秒）
        con->set_open_handshake_timeout(2000); // 5 秒

        c->connect(con);
    }
}

// 获取当前时间戳（Unix Epoch 时间戳，单位为秒）
std::string Account::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

// Base64 编码函数
std::string Account::base64Encode(const unsigned char* input, int length) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input, length);
    BIO_flush(bio);

    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length - 1); // 去掉末尾的换行符
    BIO_free_all(bio);

    return result;
}

// 生成签名字符串
std::string Account::generateSignature(const std::string& secretKey, const std::string& timestamp) {
    const std::string method = "GET";
    const std::string requestPath = "/users/self/verify";

    // 拼接字符串
    std::string data = timestamp + method + requestPath;

    // 使用 HMAC SHA256 加密
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    HMAC(
        EVP_sha256(),                          // 使用 SHA256 算法
        secretKey.c_str(), secretKey.length(), // SecretKey
        reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), // 数据
        hash, &hashLen                         // 输出
    );

    // 对加密结果进行 Base64 编码
    return base64Encode(hash, hashLen);
}

void Account::on_open(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "Connection opened." << std::endl;
    Account::is_connected = true;

    // 每次连接成功后发送一条消息
    websocketpp::lib::error_code ec;
    //std::string secretKey = "";
    std::string timestamp = getTimestamp();
    std::string sign = generateSignature(secretKey, timestamp);
    c->send(hdl, "{\"op\":\"login\",\"args\": [{\"apiKey\": \""+apiKey+"\",\"passphrase\": \"Tjs1997.\",\"timestamp\":\""+timestamp+"\",\"sign\":\""+sign+"\"}]}", websocketpp::frame::opcode::text,ec);
    if (ec) {
        std::cout << "login Send failed: " << ec.message() << std::endl;
    } else {
        std::cout << "login Sent message: OK"<< std::endl;
    }
}

void Account::on_fail(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "Connection failed or timed out." << std::endl;
    Account::is_connected = false;

    if (Account::should_reconnect) {
        std::cout << "Attempting to reconnect..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待 5 秒后重连

        websocketpp::lib::error_code ec;
        Account::con = c->get_connection(Account::wss_url, ec);
        if (ec) {
            std::cout << "Reconnect error: " << ec.message() << std::endl;
            return;
        }

        // 设置连接超时时间（例如 5 秒）
        con->set_open_handshake_timeout(2000); // 5 秒

        c->connect(con);
    }
}

int main(){
    Account account;
    Books5 bk5;
    
    //std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // 睡眠 5000 毫秒
    while (true)
    {
        /* code */
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 睡眠 200 毫秒
        std::string state = "short";
        if(state=="long"){
            if(bk5.ask1_price.load(std::memory_order_relaxed)!=0.0 && bk5.bid1_price.load(std::memory_order_relaxed)!=0.0 && account.pos_long.load(std::memory_order_relaxed)>-1.0){
            
                if(account.order_id==""){
                    if(account.pos_long.load(std::memory_order_relaxed)<0.66 && bk5.bid1_price.load(std::memory_order_relaxed)<account.long_sell_live_px.load(std::memory_order_relaxed)*(1-0.0075)){
                        auto now = std::chrono::system_clock::now();
                        account.order_id=std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
                        double ord_px = bk5.bid1_price.load(std::memory_order_relaxed);
                        std::ostringstream oss;
                        oss << std::fixed << std::setprecision(2) << ord_px; // 保留两位小数
                        account.order_px.store(ord_px,std::memory_order_relaxed);
                        account.post_order_long(account.order_id,oss.str());
                        //std::this_thread::sleep_for(std::chrono::milliseconds(60000)); // 睡眠 60*1000 毫秒
                    }
                    
                }else{
                    if(bk5.bid1_price.load(std::memory_order_relaxed)>account.order_px.load(std::memory_order_relaxed)){
                        account.cancel_order(account.order_id);
                        
                        account.order_id="";
                        account.order_px.store(0.0,std::memory_order_relaxed);
                    }else if(bk5.bid1_price.load(std::memory_order_relaxed)<account.order_px.load(std::memory_order_relaxed)){
                        account.cancel_order(account.order_id);

                        account.order_id="";
                        account.order_px.store(0.0,std::memory_order_relaxed);
                    }
                }
            }
        }else if(state=="short"){
            if(bk5.ask1_price.load(std::memory_order_relaxed)!=0.0 && bk5.bid1_price.load(std::memory_order_relaxed)!=0.0 && account.pos_short.load(std::memory_order_relaxed)>=-1.0){
            
                if(account.order_id==""){
                    if(account.pos_short.load(std::memory_order_relaxed)<0.30 && bk5.ask1_price.load(std::memory_order_relaxed)>account.short_buy_live_px.load(std::memory_order_relaxed)*(1+0.005)){
                        auto now = std::chrono::system_clock::now();
                        account.order_id=std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
                        double ord_px = bk5.ask1_price.load(std::memory_order_relaxed);
                        std::ostringstream oss;
                        oss << std::fixed << std::setprecision(2) << ord_px; // 保留两位小数
                        account.order_px.store(ord_px,std::memory_order_relaxed);
                        
                        account.post_order_short(account.order_id,oss.str());
                        //std::this_thread::sleep_for(std::chrono::milliseconds(60000)); // 睡眠 60*1000 毫秒
                    }
                    
                }else{
                    if(bk5.ask1_price.load(std::memory_order_relaxed)<account.order_px.load(std::memory_order_relaxed)){
                        account.cancel_order(account.order_id);
                        
                        account.order_id="";
                        account.order_px.store(0.0,std::memory_order_relaxed);
                    }else if(bk5.ask1_price.load(std::memory_order_relaxed)>account.order_px.load(std::memory_order_relaxed)){
                        account.cancel_order(account.order_id);

                        account.order_id="";
                        account.order_px.store(0.0,std::memory_order_relaxed);
                    }
                }
            }
        }
        
    }
    
    account.wait();
    bk5.wait();
    return 0;
}