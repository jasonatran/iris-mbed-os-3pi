#include "range.h"

#define DEBUG   1

#if (DEBUG) 
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif /* (DEBUG) & DEBUG_PRINT */

static volatile bool ranging = 0;

static node_t nodes_discovered[MAX_NUM_ANCHORS];
static uint8_t num_nodes_discovered;
static uint8_t num_nodes_to_pub;
static char EMCUTE_ID[9];
static uint8_t last_node_locked = 0;
static uint16_t range_riot_port = RIOT_MQTT_PORT;
static int mqtt_is_on = 1; /* on by default */
static char hdlc_initialized = 0;
static char thread_initialized = 0;

Mail<msg_t, HDLC_MAILBOX_SIZE>  range_thr_mailbox;
Thread range_thr;

hdlc_entry_t range_thread = { NULL, MBED_RANGE_PORT, &range_thr_mailbox };

void range_load_id(hdlc_buf_t *buff){
    memcpy(EMCUTE_ID, &(buff->data) + UART_PKT_HDR_LEN, ID_LENGTH);
    PRINTF("ID: %s\n",EMCUTE_ID);
}

node_t get_node(range_data_t data){
    return (node_t){data.node_id, data.tdoa};
}

int load_node_data(char *buff, size_t buff_size, node_t node){
    return load_data(buff, buff_size, node, NODE_DATA_FLAG);
}

int load_discovered_nodes(char *buff, size_t buff_size){
    return load_data(buff, buff_size, (node_t) {0,0}, NODE_DISC_FLAG);
}

int load_data(char *buff, size_t buff_size, node_t node, int flag){
    if(flag == NODE_DATA_FLAG){
        if(buff[ID_LENGTH] == NODE_DISC_FLAG + 0x30){ 
            PRINTF("Buffer is already being used for node discovery, you must clear the buffer first\n");
            return -1;
        }

        //if((num_nodes_to_pub + 1) * DATA_STRING_SIZE + ID_LENGTH + 1 >= buff_size - 1){
        if((num_nodes_to_pub + 1) * sizeof(node_t) + ID_LENGTH + 1 >= buff_size - 1){
            PRINTF("Buffer is full\n");
            return -1;
        }

        if(num_nodes_to_pub == 0){
            memcpy(buff, EMCUTE_ID, ID_LENGTH);
            buff[ID_LENGTH] = flag+0x30;
        }

        //snprintf(buff + (num_nodes_to_pub * DATA_STRING_SIZE) + ID_LENGTH + 1, buff_size - (num_nodes_to_pub * DATA_STRING_SIZE) - ID_LENGTH - 1, "%05d,%02d;", node.tdoa, node.node_id);
        buff[ID_LENGTH + 1] = num_nodes_to_pub + 1; 
        PRINTF("%d - %d\n", node.node_id, node.tdoa);
        memcpy(buff + (num_nodes_to_pub * sizeof(node_t)) + ID_LENGTH + 2, &node, sizeof(node_t));
        
        num_nodes_to_pub++;
        PRINTF("# of nodes = %d\n",num_nodes_to_pub);
        return (num_nodes_to_pub * sizeof(node_t)) + ID_LENGTH + 2;
    }
    else if(flag == NODE_DISC_FLAG){
        int i;
        if(buff[ID_LENGTH] == NODE_DATA_FLAG + 0x30){
            PRINTF("Buffer is already being used for node data, you must clear the buffer first\n");
            return -1;
        }

        //if((num_nodes_discovered * LOAD_DISC_NODE_LENG) + ID_LENGTH + 1 >= buff_size - 1){
        if((num_nodes_discovered * sizeof(uint8_t)) + ID_LENGTH + 1 >= buff_size - 1){
            PRINTF("Buffer is not big enough\n");
            return -1;
        }

        memcpy(buff, EMCUTE_ID, ID_LENGTH);
        buff[ID_LENGTH] = flag+0x30;

        buff[ID_LENGTH + 1] = num_nodes_discovered; 

        for(i=0; i < num_nodes_discovered; i++){
            //snprintf(buff + (i * LOAD_DISC_NODE_LENG) + ID_LENGTH + 1, buff_size - (i * LOAD_DISC_NODE_LENG) - ID_LENGTH - 1, "%02d,", nodes_discovered[i].node_id);
            buff [i * sizeof(uint8_t) + ID_LENGTH + 2] = nodes_discovered[i].node_id;
        }
        PRINTF("# of nodes discovered = %d\n",num_nodes_discovered);
        return num_nodes_discovered * sizeof(uint8_t) + ID_LENGTH + 2;
    }
    else{
        return -1;
    }
}

void clear_data(char *buff, size_t buff_size){
    memset(buff,0,buff_size);
    num_nodes_to_pub = 0;
}

dist_angle_t get_dist_angle(range_data_t *time_diffs, uint8_t ranging_mode){
    int tdoa_a = 0;
    int tdoa_b = 0;
    float dist_a = 0;
    float dist_b = 0;
    float dist = 0;
    float angle = -361;

    dist_angle_t return_val;
    return_val.distance = -1;
    return_val.angle = 0;
    return_val.node_id = 0;

    tdoa_a = time_diffs->tdoa;
    dist_a = get_dist(tdoa_a);

    switch (ranging_mode)
    {
        case ONE_SENSOR_MODE:
            dist = dist_a;
            break;
        case TWO_SENSOR_MODE:
            if(time_diffs->status > 2)
            {
                PRINTF("Missed pin %d\n", MISSED_PIN_UNMASK - time_diffs-> status); 
            } 
            else
            {
                tdoa_b = time_diffs->tdoa + time_diffs->orient_diff;
                dist_b = get_dist(tdoa_b);
                PRINTF("OD = %d\n", time_diffs-> orient_diff);
            }
            break;
        case XOR_SENSOR_MODE:
            tdoa_b = time_diffs->tdoa + time_diffs->orient_diff;
            dist_b = get_dist(tdoa_b);
            PRINTF("OD = %d\n", time_diffs->orient_diff);
            break;
        case OMNI_SENSOR_MODE:
            dist = dist_a;
            break;
    }
    if(dist_b != 0){
        dist = get_mid_dist(dist_a, dist_b);
        if(time_diffs->status == 2){
            angle = get_angle(dist_b, dist_a);
        }
        else if(time_diffs->status == 1){
            angle = get_angle(dist_a, dist_b);
        }

        PRINTF("Distance: %.2f\n", dist);
        PRINTF("Angle : %.2f\n", angle);
    }
    else{
        PRINTF("Distance: %.2f\n", dist);
    }
     printf("******************************\n");

    return_val.distance = dist;
    return_val.angle = angle;
    return_val.node_id = time_diffs->node_id;

    printf("Range: Returning value\n");
    printf("Range: %.2f, %.2f, %d\n",return_val.distance,return_val.angle,return_val.node_id);
    return return_val;
}

range_data_t get_range_data(range_params_t params){
    Mail<msg_t, HDLC_MAILBOX_SIZE> *hdlc_mailbox_ptr = get_hdlc_mailbox();
 
    int exit = 0;
    int pkt_size = sizeof(uart_pkt_hdr_t) + sizeof(range_params_t);
    char send_data[pkt_size];

    msg_t *msg = NULL, *msg2 = NULL;
    hdlc_buf_t *buf = NULL;
    hdlc_pkt_t pkt;
    
    uart_pkt_hdr_t recv_hdr;
    uart_pkt_hdr_t send_hdr = (uart_pkt_hdr_t){ MBED_RANGE_PORT, range_riot_port, SOUND_RANGE_REQ };
    
    /* misc */
    osEvent evt;

    range_data_t* time_diffs;
    range_hdr_t* range_hdr;
    
    int i = 0;
    int j = 0;
    int data_per_pkt;

    //send ranging request packet up hdlc   
    pkt.data = send_data;
    pkt.length = pkt_size;   
    uart_pkt_insert_hdr(pkt.data, pkt.length, &send_hdr); 
    uart_pkt_cpy_data(pkt.data, pkt.length, &params, sizeof(range_params_t));

    PRINTF("src_port: %d, dst_port: %d\n", send_hdr.src_port, send_hdr.dst_port);
    PRINTF("node_id: %d\n", params.node_id);

    if (send_hdlc_mail(msg, HDLC_MSG_SND, &range_thr_mailbox, (void*) &pkt)){
        PRINTF("range_thread: sending range_req pkt\n"); 
    }
    else{
        PRINTF("range_thread: failed to send pkt no\n"); 
        return range_data_t{0,0,0,params.node_id};
    }
    //recieving data
    while(!exit)
    {
        PRINTF("range_thread: Waiting for response\n");
        evt = range_thr_mailbox.get();
        
        if(evt.status == osEventMail)
        {
            PRINTF("range_thread: Got a response\n");
            msg = (msg_t*)evt.value.p;

            switch (msg->type)
            {
                case HDLC_RESP_SND_SUCC:
                    PRINTF("range_thread: sent frame!\n");
                    range_thr_mailbox.free(msg);
                    break;
                case HDLC_RESP_RETRY_W_TIMEO:
                    Thread::wait(msg->content.value/1000);
                    PRINTF("range_thread: retrying\n");

                    /* Tries to allocate memory for msg2. */
                    msg2 = hdlc_mailbox_ptr->alloc();
                    if(msg2 == NULL) 
                    {
                        /* Blocking call until the memory has been allocated. */
                        // Thread::wait(50);
                        while(msg2 == NULL)
                        {
                            msg2 = range_thr_mailbox.alloc();  
                            Thread::wait(10);
                        }
                        msg2->type = HDLC_RESP_RETRY_W_TIMEO;
                        msg2->content.value = (uint32_t) HDLC_RTRY_TIMEO_USEC;
                        msg2->sender_pid = osThreadGetId();
                        msg2->source_mailbox = &range_thr_mailbox;
                        range_thr_mailbox.put(msg2);
                    }
                    else
                    {
                        msg2->type = HDLC_MSG_SND;
                        msg2->content.ptr = &pkt;
                        msg2->sender_pid = osThreadGetId();
                        msg2->source_mailbox = &range_thr_mailbox;
                        hdlc_mailbox_ptr->put(msg2);
                    }
                    range_thr_mailbox.free(msg);
                    break;

                case HDLC_PKT_RDY:

                    /* Setting up buf, making it easier to access data from the msg. */ 
                    buf = (hdlc_buf_t *)msg->content.ptr;   
                    uart_pkt_parse_hdr(&recv_hdr, buf->data, buf->length);

                    if(recv_hdr.pkt_type == SOUND_RANGE_DONE) 
                    {
                        PRINTF("range_thread: received range pkt\n");
                        range_hdr = (range_hdr_t *)uart_pkt_get_data(buf->data, buf->length);
                        time_diffs = (range_data_t *)range_hdr->data;
                        PRINTF("status: %d\n", time_diffs->status);
                        
                        data_per_pkt = (buf->length - sizeof(uart_pkt_hdr_t) - sizeof(uint8_t))/sizeof(range_data_t);
                        PRINTF("range_thread: There should be %d ranges in this pkt\n",data_per_pkt);

                        if(data_per_pkt == 0){
                            hdlc_pkt_release(buf);
                            range_thr_mailbox.free(msg);
                            return (range_data_t){0,0,0,params.node_id};
                        }

                        for(i = 0; i < data_per_pkt; i++){
                            PRINTF ("%d:\n", i);
                            /* Displaying results. */
                            
                            PRINTF("TDoA = %d\n", time_diffs->tdoa);

                            switch (params.ranging_mode)
                            {
                                case ONE_SENSOR_MODE:
                                    PRINTF("One Sensor Mode^\n");
                                    break;
                                case TWO_SENSOR_MODE:
                                    if(time_diffs->status > 2){
                                        printf("Missed pin %d\n", MISSED_PIN_UNMASK - time_diffs->status); 
                                    } 
                                    else{
                                        PRINTF("OD = %d\n", time_diffs->orient_diff);
                                    }
                                    PRINTF("Two Sensor Mode^\n");
                                    break;
                                case XOR_SENSOR_MODE:
                                    PRINTF("OD = %d\n", time_diffs->orient_diff);
                                    PRINTF("Xor Sensor Mode^\n");
                                    break;
                                case OMNI_SENSOR_MODE:
                                    PRINTF("Omni Sensor Mode^\n");
                                    break;
                            }

                            if(params.node_id == -1){
                                nodes_discovered[j] = (node_t) {time_diffs->node_id, time_diffs->tdoa};
                                j++;
                                if(j >= MAX_NUM_ANCHORS){
                                    printf("Exceeded max number of anchors\n");
                                    return (range_data_t){0,0,0,params.node_id};
                                }
                                num_nodes_discovered = j;
                            }
                            time_diffs++;
                        }
                        if(range_hdr->last_pkt == 1){
                            printf("All data recieved\n");
                            exit = 1;
                        }
                    }
                    else
                    {
                        printf("main_thr: recieved non-range pkt\n");
                        exit = 1;
                    }
                    hdlc_pkt_release(buf);
                    range_thr_mailbox.free(msg);
                    break;
                default:
                    PRINTF("Reached default case\n");
                    /* error */
                    //LED3_ON;
                    range_thr_mailbox.free(msg);
                    break;
            }
        } 
        else{
            printf("Range: Didn't get mail: %02x\n",evt.status);
        }   
        if(exit) 
        {
            printf("Range: Exiting loop\n");
            exit = 0;
            break;
        }
    }
    
    ranging = 0;

    if(params.node_id == -1){
        return (range_data_t){0,0,0,params.node_id};
    }
    else{
        return *(time_diffs-1);
    }
    
}

void discover_nodes(uint8_t ranging_mode){
    get_range_data((range_params_t){-1, ranging_mode});
}

/* TODO: Do we need a wrapper for this function */
range_data_t range_node(range_params_t params){
    return get_range_data(params);
}


/**
 * @brief      This is the range thread that triggers the range routine for localization.
 *             To trigger localization, a msg of type START_RANGE_THR must be sent to the
 *             range_thread_mailbox containing the parameters for localization.
 */
void _range_thread(){

    char            data_pub[32];
    hdlc_pkt_t      pkt;
    pkt.data = data_pub;
    pkt.length = 32;
    mqtt_pkt_t      mqtt_send;


    int mqtt_data_len;
    int i = 0;
    
    osEvent evt;
    msg_t *msg;

    range_data_t range_data;
    range_params_t range_params;

    // hdlc_entry_t range_thread = { NULL, MBED_RANGE_PORT, &range_thr_mailbox };
    // hdlc_register(&range_thread);

    thread_initialized = 1;

    while(1)
    {

        PRINTF("range_thread: Waiting for start message\n");
        evt = range_thr_mailbox.get();
        if(evt.status == osEventMail)
        {
            PRINTF("range_thread: got mail\n");
            msg = (msg_t*)evt.value.p;
            if(msg->type == START_RANGE_THR){ 
                ranging = 1;

                //***This is where the range routine will go*****
                
                PRINTF("range_thread: got range init message, starting routine\n");
                range_params = *(range_params_t*)(msg->content.ptr);

                if(range_params.node_id == -1){
                    discover_nodes(range_params.ranging_mode);
                    clear_data(data_pub, 32);

                    printf("****************Discovery mode***************\n");
                    printf("Nodes reached:\n");
                    for(i=0; i<num_nodes_discovered; i++){
                        printf("Node %d: %d\n", nodes_discovered[i].node_id, nodes_discovered[i].tdoa);
                        mqtt_data_len = load_node_data(data_pub, 32, nodes_discovered[i]); 
                    }
                    printf("*********************************************\n");
                    
                    //mqtt_data_len = load_discovered_nodes(data_pub, 32); //loads only node_id
                    
                    printf("size: %d", mqtt_data_len);
                    if(mqtt_data_len != -1){
                        build_mqtt_pkt_npub(RANGE_TOPIC, data_pub, MBED_MQTT_PORT, &mqtt_send, mqtt_data_len, &pkt); 
                        
                        //for some reason it will only publish if you include a print statement here
                        PRINTF("range_thread: range_routine done. publishing data now\n");

                        if (send_hdlc_mail(msg, HDLC_MSG_SND, &range_thr_mailbox, (void*) &pkt)){
                            PRINTF("mqtt_thread: sending pkt of size\n"); 
                        }
                        else{
                            PRINTF("mqtt_thread: failed to send pkt no\n"); 
                        }
                    }
                    else{
                        PRINTF ("Failed to load discovered nodes\n");
                    }

                }
                else{
                    range_data = range_node(range_params);
                    
                    /*if(range_params.ranging_mode == TWO_SENSOR_MODE){
                        
                        //Lock on anchor code goes here

                    }
                    else{
                         range_data = range_node(range_params);
                    }*/

                    if(mqtt_is_on) {
                        clear_data(data_pub, 32);
                        mqtt_data_len = load_node_data(data_pub, 32, get_node(range_data));
                        if(mqtt_data_len != -1){

                            build_mqtt_pkt_npub(RANGE_TOPIC, data_pub, MBED_MQTT_PORT, &mqtt_send, mqtt_data_len, &pkt); 
                            
                            //for some reason it will only publish if you include a print statement here
                            printf("tdoa = %d\n",range_data.tdoa);
                            printf("node_id = %d\n",range_data.node_id);
                            PRINTF("range_thread: range_routine done. publishing data now\n");

                            if (send_hdlc_mail(msg, HDLC_MSG_SND, &range_thr_mailbox, (void*) &pkt)){
                                PRINTF("mqtt_thread: sending pkt of size\n"); 
                            }
                            else{

                                PRINTF("mqtt_thread: failed to send pkt no\n"); 
                            }
                        }
                        else{
                            PRINTF ("Failed to load node data\n");
                        }
                    } 
                    else {
                        /* TODO: THIS IS APPLICATION SPECIFIC -- need adaptation 
                        for other modes */ 
                        if (range_data.tdoa != 0) {
                            /* send the results back to the requesing thread */
                            Mail<msg_t, HDLC_MAILBOX_SIZE> *src_mailbox = 
                                (Mail<msg_t, HDLC_MAILBOX_SIZE> *)msg->source_mailbox;
                            msg = src_mailbox->alloc();
                            msg->type = RANGING_DONE;
                            msg->content.ptr = &range_data;
                            msg->sender_pid = osThreadGetId();
                            msg->source_mailbox = &range_thr_mailbox;
                            src_mailbox->put(msg);
                        }

                        mqtt_is_on = 1;
                    }

                }
                ranging = 0;
                PRINTF("SETTING SIGNAL\n");
                //*************************************************
            }
            else{
                PRINTF("range_thread: Recieved something other than start message: %d\n", msg->type);
                range_thr_mailbox.free(msg);
                
                continue;
            }
        } 
        else{
            range_thr_mailbox.free(msg);
            printf("range_thread: Didn't get mail: %02x\n",evt.status);
            continue;
        }  
        range_thr_mailbox.free(msg);
    }
    
}

void init_range(int flag){
    if(hdlc_initialized == 0){
        hdlc_register(&range_thread);
        PRINTF("Registering range_thread");
        if (flag == 1){
            range_thr.start(_range_thread);
        }
        hdlc_initialized = 1;
    }    
}

/* NOTE: NOTE THREAD SAFE. If another thread requests ranging before the thread
before it processes the range results, the range results will be overwritten
by the new ranging result. See range thread code where the results are sent
back to the requesting thread via Mail */
void trigger_range_routine(range_params_t *params, msg_t *msg, 
                           Mail<msg_t, HDLC_MAILBOX_SIZE> *src_mailbox){
    mqtt_is_on = 0;
    if(!ranging){
        msg = range_thr_mailbox.alloc();
        msg->type = START_RANGE_THR;
        msg->content.ptr = params;
        msg->sender_pid = osThreadGetId();
        msg->source_mailbox = src_mailbox;
        range_thr_mailbox.put(msg);
    }
}

int trigger_range_routine_blocking(range_params_t *params, msg_t *msg){
    if(!ranging && thread_initialized){
        msg = range_thr_mailbox.alloc();
        msg->type = START_RANGE_THR;
        msg->content.ptr = params;
        msg->sender_pid = osThreadGetId();
        msg->source_mailbox = &range_thr_mailbox;
        range_thr_mailbox.put(msg);
        ranging = 1;
    }
    else{
        return 0;
    }
    PRINTF("******RANGE BLOCKING********\n");
    // Thread::signal_wait(0x1);
    while(ranging){Thread::wait(100);};
    PRINTF("***********DONE*************\n");
    return 1;
}

bool is_ranging(){
    if(ranging){
        return 1;
    }
    else{
        return 0;
    }
}

node_t* get_nodes_discovered(){
    return nodes_discovered;
}

uint8_t get_num_nodes_discovered(){
    return num_nodes_discovered;
}

void set_range_riot_port(uint16_t port){
    range_riot_port = port;
}
