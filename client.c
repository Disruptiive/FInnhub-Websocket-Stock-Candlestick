#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libwebsockets.h>
#include <sys/time.h>
#include <stdbool.h>
#include <cjson/cJSON.h>


#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define KBRN "\033[0;33m"
#define RESET "\033[0m"
#define EXAMPLE_RX_BUFFER_BYTES (100)

typedef struct{
    bool empty;
    char *symbol;
    double open_price;
    double close_price;
    double low_price;
    double high_price;
    double total_volume;
    double total_price;
    int count;
} candle_t;

typedef struct{
    char *symbol;
    double total_volume;
    double total_price;
    int count;
} per_min_stats_t;

typedef struct{
    per_min_stats_t **past_stats;
    candle_t *candlesticks;
    char **symbol_arr;
    int symbol_num;
    int past_minutes;
    bool sent;
} data_t;

data_t* initialiseData(char stocks[][100],int n_stocks,int past_minutes){
    data_t* s = malloc(sizeof(data_t));
    s->symbol_num = n_stocks;
    s->past_minutes = past_minutes;
    char** syms = malloc(sizeof(char*)*s->symbol_num);
    for (int i=0;i<s->symbol_num;i++){
        syms[i] = &stocks[i][0];
    }
    s->symbol_arr = syms;
    s->sent = false;
    s->past_stats = malloc(sizeof(per_min_stats_t*)*s->symbol_num);
    s->candlesticks = malloc(sizeof(candle_t)*s->symbol_num);
    for (int i=0;i<s->symbol_num;i++){
        s->candlesticks[i].symbol = &stocks[i][0];
        s->candlesticks[i].empty = true;
        s->past_stats[i] = malloc(sizeof(per_min_stats_t)*s->past_minutes);
        for(int j=0;j<s->past_minutes;j++){
            s->past_stats[i][j].total_volume = 0;
            s->past_stats[i][j].total_price = 0;
            s->past_stats[i][j].count = 0;
            s->past_stats[i][j].symbol = &stocks[i][0];
        }
    }
    return s;
}

int findIndex(data_t* data, char* symbol){
    for(int i=0;i<data->symbol_num;i++){
        if (strcmp(data->candlesticks[i].symbol,symbol) == 0){
            return i;
        }
    }
    return -1;
}

unsigned long long returnTime(){
    struct timeval tv;

    gettimeofday(&tv, NULL);

    unsigned long long time =(unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    return time;
}

void writeTrade(char* symbol,double p, double v,unsigned long long trade_time, int time_diff){
    char name[30];
    sprintf(name,"%s-Trades.txt",symbol);
    
    FILE *f;
    f = fopen(name,"a");
    size_t needed = snprintf(NULL, 0, "%lf %lf %llu %d\n",p,v,trade_time,time_diff) + 1;
    char  *buffer = malloc(needed);
    sprintf(buffer,"%lf %lf %llu %d\n",p,v,trade_time,time_diff);
    fwrite(buffer,needed,1,f);
    free(buffer);
    fclose(f);

}

void updateCandlestick(data_t* data,int idx, double p, unsigned long long t, double v){
    if (data->candlesticks[idx].empty){
        data->candlesticks[idx].open_price = p;
        data->candlesticks[idx].close_price = p;
        data->candlesticks[idx].low_price = p;
        data->candlesticks[idx].high_price = p;
        data->candlesticks[idx].total_price = p;
        data->candlesticks[idx].total_volume = v;
        data->candlesticks[idx].empty = false;
        data->candlesticks[idx].count = 1;
    }
    else{
        data->candlesticks[idx].close_price = p;
        if (p>data->candlesticks[idx].high_price){
            data->candlesticks[idx].high_price = p;
        }
        if (p<data->candlesticks[idx].high_price){
            data->candlesticks[idx].low_price = p;
        }
        data->candlesticks[idx].total_price += p;
        data->candlesticks[idx].total_volume += v; 
        data->candlesticks[idx].count += 1;
    }

    unsigned long long time_now = returnTime();
    int time_diff = time_now - t;
    
    writeTrade(data->candlesticks[idx].symbol,p,v,t,time_diff);
    //printf("Candlestick: %s Open: %lf Close: %lf Low: %lf High: %lf Total Price: %lf Total Volume: %lf Timediff: %llu Empty: %d Count: %d\n", data->candlesticks[idx].symbol, data->candlesticks[idx].open_price,data->candlesticks[idx].close_price,data->candlesticks[idx].low_price,data->candlesticks[idx].high_price,data->candlesticks[idx].total_price,data->candlesticks[idx].total_volume,time_diff,data->candlesticks[idx].empty,data->candlesticks[idx].count);
}

void writeCandlestick(candle_t candle){
    char name[30];
    sprintf(name,"%s-Candlesticks.txt",candle.symbol);
    
    FILE *f;
    f = fopen(name,"a");
    size_t needed = snprintf(NULL, 0, "%lf %lf %lf %lf %lf \n",candle.open_price, candle.close_price, candle.low_price, candle.high_price, candle.total_volume) + 1;
    char  *buffer = malloc(needed);
    sprintf(buffer,"%lf %lf %lf %lf %lf \n",candle.open_price, candle.close_price, candle.low_price, candle.high_price, candle.total_volume);
    fwrite(buffer,needed,1,f);
    free(buffer);
    fclose(f);
}

void writeData(char *symbol, double avg_price, double total_volume){
    char name[30];
    sprintf(name,"%s-Data.txt",symbol);
    
    FILE *f;
    f = fopen(name,"a");
    size_t needed = snprintf(NULL, 0, "%lf %lf\n",avg_price,total_volume) + 1;
    char  *buffer = malloc(needed);
    sprintf(buffer,"%lf %lf\n",avg_price,total_volume);
    fwrite(buffer,needed,1,f);
    free(buffer);
    fclose(f);
}

void calculatePastData(data_t* data){
    for(int i=data->symbol_num-1;i>=0;i--){
        double total_volume = 0;
        double total_price = 0;
        int count = 1;
        for(int j=data->past_minutes-1;j>=0;j--){
            if (j!=0){
                data->past_stats[i][j] = data->past_stats[i][j-1];
            }
            else{
                if(data->candlesticks[i].empty){
                    data->past_stats[i][j].total_volume = 0;
                    data->past_stats[i][j].total_price = 0;
                    data->past_stats[i][j].count = 0;
                }
                else{
                    data->past_stats[i][j].total_volume = data->candlesticks[i].total_volume;
                    data->past_stats[i][j].total_price = data->candlesticks[i].total_price;
                    data->past_stats[i][j].count = data->candlesticks[i].count;
                }
                
            }
            total_volume += data->past_stats[i][j].total_volume;
            total_price += data->past_stats[i][j].total_price;
            count += data->past_stats[i][j].count;
        }
        double avg_price = total_price/count;
        if (count > 1)
            writeData(data->candlesticks[i].symbol,avg_price,total_volume);
    }
}

void calculateCandlestick(candle_t *candlesticks,int n_stocks){
    for(int i=0;i<n_stocks;i++){
        if (!candlesticks[i].empty){
            candlesticks[i].empty = true;
            writeCandlestick(candlesticks[i]);
        }
    }
}


void parseJson(data_t* data, char* response){
    cJSON *resp = cJSON_Parse(response);
    const char *trade_str = "trade";
    const cJSON *type = NULL;
    const cJSON *trades = NULL;
    const cJSON *trade = NULL;

    int cnt = 0;
    if (resp == NULL)
    {
        printf("errored resp");
        cJSON_Delete(resp);
        return;
    }
    type = cJSON_GetObjectItemCaseSensitive(resp, "type");
    if (strcmp(type->valuestring,trade_str) == 0)
    {
        trades = cJSON_GetObjectItemCaseSensitive(resp,"data");
        cJSON_ArrayForEach(trade,trades)
        {
            cJSON *p = cJSON_GetObjectItemCaseSensitive(trade, "p");
            cJSON *s = cJSON_GetObjectItemCaseSensitive(trade, "s");
            cJSON *t = cJSON_GetObjectItemCaseSensitive(trade, "t");
            cJSON *v = cJSON_GetObjectItemCaseSensitive(trade, "v");

            //printf("%lf-%s-%llu-%lf-%d\n",p->valuedouble,s->valuestring,t->valuedouble,v->valuedouble,cnt++);
            int idx = findIndex(data,s->valuestring);
            updateCandlestick(data,idx,p->valuedouble,(unsigned long long)t->valuedouble,v->valuedouble);
        }
    }
    cJSON_Delete(resp);
}

static int destroy_flag = 0;
static int connection_flag = 0;
static int writeable_flag = 0;

static void INT_HANDLER(int signo) {
    destroy_flag = 1;
}

struct session_data {
    int fd;
};

static int ws_service_callback(
                         struct lws *wsi,
                         enum lws_callback_reasons reason, void *user,
                         void *in, size_t len)
{
    
    void *userdata = lws_context_user(lws_get_context(wsi));

    switch (reason) {

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lws_set_timer_usecs(wsi,60000000);
            printf(KYEL"[Main Service] Connect with server success.\n"RESET);
            connection_flag = 1;
	        lws_callback_on_writable(wsi);
            break;
        
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf(KRED"[Main Service] Connect with server error: %s.\n"RESET, in);
            destroy_flag = 1;
            connection_flag = 0;
            break;
        
        case LWS_CALLBACK_CLOSED:
            printf(KYEL"[Main Service] LWS_CALLBACK_CLOSED\n"RESET);
            exit(0);
            destroy_flag = 1;
            connection_flag = 0;
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf(KYEL"\nSESSION ENDED\n"RESET);
            exit(0);
            destroy_flag = 1;
            connection_flag = 0;
            break;

        case LWS_CALLBACK_TIMER:
            lws_set_timer_usecs(wsi,60000000);
            calculatePastData(((data_t*)userdata));
            calculateCandlestick(((data_t*)userdata)->candlesticks,((data_t*)userdata)->symbol_num);
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            //printf(KCYN_L"[Main Service] Client recvived:%s\n"RESET, (char *)in);
            parseJson((data_t*)userdata,(char *)in);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if(!((data_t*)userdata)->sent){
                printf(KYEL"[Main Service] On writeable is called.\n"RESET);
                char* out = NULL;
                char str[50];
                for(int i = 0; i < ((data_t*)userdata)->symbol_num ; i++)
                {
                sprintf(str, "{\"type\":\"subscribe\",\"symbol\":\"%s\"}", ((data_t*)userdata)->symbol_arr[i]);
                printf(str);
                int len = strlen(str);
                
                out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
                memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
                
                lws_write(wsi, out+LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);
                }
                free(out);
                writeable_flag = 1;
                ((data_t*)userdata)->sent = true;
                break;
            }
            else break;

        default:
            break;
    }

    return 0;
}


static struct lws_protocols protocols[] =
{
	{
		"example-protocol",
		ws_service_callback,
        sizeof(data_t),
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

int main(void)
{   
    //* register the signal SIGINT handler */
    struct sigaction act;
    act.sa_handler = INT_HANDLER;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction( SIGINT, &act, 0);


    struct lws_context *context = NULL;
    struct lws_context_creation_info info;
    struct lws *wsi = NULL;
    struct lws_protocols protocol;

    char symb_arr[][100] = {"BA\0", "AMZN\0", "IC MARKETS:1\0", "BINANCE:BTCUSDT\0","AAPL\0","NVDA\0"};
    int n_stocks = sizeof(symb_arr)/sizeof(symb_arr[0]);
    int minutes = 15;
    data_t* p = initialiseData(symb_arr,n_stocks,minutes);
    void *t;
    t = p;
    //lws_set_log_level(1151, NULL);
    memset(&info, 0, sizeof info);

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    info.user = t;

    
    char inputURL[300] = 
	"wss://ws.finnhub.io/?token=cb69tmqad3i70tu6288g";
    const char *urlProtocol, *urlTempPath;
	char urlPath[300];
    context = lws_create_context(&info);
    printf(KRED"[Main] context created.\n"RESET);

    if (context == NULL) {
        printf(KRED"[Main] context is NULL.\n"RESET);
        return -1;
    }
    struct lws_client_connect_info clientConnectionInfo;
    memset(&clientConnectionInfo, 0, sizeof(clientConnectionInfo));
    clientConnectionInfo.context = context;
    if (lws_parse_uri(inputURL, &urlProtocol, &clientConnectionInfo.address,
	    &clientConnectionInfo.port, &urlTempPath))
    {
	    printf("Couldn't parse URL\n");
    }

    urlPath[0] = '/';
    strncpy(urlPath + 1, urlTempPath, sizeof(urlPath) - 2);
    urlPath[sizeof(urlPath)-1] = '\0';
    clientConnectionInfo.port = 443;
    clientConnectionInfo.path = urlPath;
    clientConnectionInfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    clientConnectionInfo.host = clientConnectionInfo.address;
    clientConnectionInfo.origin = clientConnectionInfo.address;
    clientConnectionInfo.ietf_version_or_minus_one = -1;
    clientConnectionInfo.protocol = "";
    clientConnectionInfo.local_protocol_name = protocols[0].name;
    printf("Testing %s\n\n", clientConnectionInfo.address);
    printf("Connecticting to %s://%s:%d%s \n\n", urlProtocol, 
    clientConnectionInfo.address, clientConnectionInfo.port, urlPath);

    
    wsi = lws_client_connect_via_info(&clientConnectionInfo);

    if (wsi == NULL) {
        printf(KRED"[Main] wsi create error.\n"RESET);
        return -1;
    }

    printf(KGRN"[Main] wsi create success.\n"RESET);

    while(!destroy_flag)
    {
        lws_service(context, 0);
    }

    lws_context_destroy(context);

    
}
