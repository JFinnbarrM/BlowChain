

#include <zephyr/net/http/client.h>

static void http_response_cb(struct http_response *rsp, 
                           enum http_final_call final_data, 
                           void *user_data)
{
    if (final_data == HTTP_DATA_MORE) {
        printk("Partial data received (%zd bytes)\n", rsp->data_len);
    } else if (final_data == HTTP_DATA_FINAL) {
        printk("All data received (%zd bytes)\n", rsp->data_len);
    }
    
    printk("Response status %d\n", rsp->http_status);
    printk("Body: %.*s\n", rsp->data_len, rsp->recv_buf);
}

static int send_http_request(struct tago_data *data)
{
    struct http_request req = {0};
    char payload[512];
    
    // Create JSON payload (same as before)
    
    req.method = HTTP_POST;
    req.url = "/data";
    req.host = TAGO_URL;
    req.protocol = "HTTPS";
    req.response = http_response_cb;
    req.payload = payload;
    req.payload_len = strlen(payload);
    req.header_fields = "Device-Token: " TAGO_DEVICE_TOKEN "\r\n"
                       "Content-Type: application/json\r\n";
    
    return http_client_req(NULL, &req, 5000, NULL);
}