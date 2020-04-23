#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

static const char *urls[] = {
    "https://cdn.betterttv.net/emote/54fa92ee01e468494b85b553/3x",
    "https://cdn.betterttv.net/emote/54fa928f01e468494b85b54f/3x",
    "https://cdn.betterttv.net/emote/54fa90ba01e468494b85b543/3x",
    "https://cdn.betterttv.net/emote/54fa9cc901e468494b85b565/3x",
    "https://cdn.betterttv.net/emote/54fb603201abde735115ddb5/3x",
    "https://cdn.betterttv.net/emote/54fa913701e468494b85b546/3x",
    "https://cdn.betterttv.net/emote/54fa935601e468494b85b557/3x",
    "https://cdn.betterttv.net/emote/54fa909b01e468494b85b542/3x",
    "https://cdn.betterttv.net/emote/54fa919901e468494b85b548/3x",
    "https://cdn.betterttv.net/emote/54fa930801e468494b85b554/3x",
    "https://cdn.betterttv.net/emote/54fa90f201e468494b85b545/3x",
    "https://cdn.betterttv.net/emote/54fa934001e468494b85b556/3x",
    "https://cdn.betterttv.net/emote/54fa99b601e468494b85b55d/3x",
    "https://cdn.betterttv.net/emote/54fa903b01e468494b85b53f/3x",
    "https://cdn.betterttv.net/emote/54fa932201e468494b85b555/3x",
    "https://cdn.betterttv.net/emote/54fab45f633595ca4c713abc/3x",
    "https://cdn.betterttv.net/emote/54fa8fce01e468494b85b53c/3x",
    "https://cdn.betterttv.net/emote/5622aaef3286c42e57d8e4ab/3x",
    "https://cdn.betterttv.net/emote/54fab7d2633595ca4c713abf/3x",
    "https://cdn.betterttv.net/emote/54fa8f1401e468494b85b537/3x",
    "https://cdn.betterttv.net/emote/566ca11a65dbbdab32ec0558/3x",
    "https://cdn.betterttv.net/emote/54fbef6601abde735115de57/3x",
    "https://cdn.betterttv.net/emote/54fbefc901abde735115de5a/3x",
    "https://cdn.betterttv.net/emote/54fbf02f01abde735115de5d/3x",
    "https://cdn.betterttv.net/emote/54fbefeb01abde735115de5b/3x",
    "https://cdn.betterttv.net/emote/54fbf07e01abde735115de5f/3x",
    "https://cdn.betterttv.net/emote/553b48a21f145f087fc15ca6/3x",
    "https://cdn.betterttv.net/emote/566ca1a365dbbdab32ec055b/3x",
    "https://cdn.betterttv.net/emote/54fbf09c01abde735115de61/3x",
    "https://cdn.betterttv.net/emote/54fbf00a01abde735115de5c/3x",
    "https://cdn.betterttv.net/emote/54fbf05a01abde735115de5e/3x",
    "https://cdn.betterttv.net/emote/555015b77676617e17dd2e8e/3x",
    "https://cdn.betterttv.net/emote/55471c2789d53f2d12781713/3x",
    "https://cdn.betterttv.net/emote/54fbef8701abde735115de58/3x",
    "https://cdn.betterttv.net/emote/555981336ba1901877765555/3x",
    "https://cdn.betterttv.net/emote/55f324c47f08be9f0a63cce0/3x",
    "https://cdn.betterttv.net/emote/55b6524154eefd53777b2580/3x",
    "https://cdn.betterttv.net/emote/566c9fc265dbbdab32ec053b/3x",
    "https://cdn.betterttv.net/emote/560577560874de34757d2dc0/3x",
    "https://cdn.betterttv.net/emote/566c9fde65dbbdab32ec053e/3x",
    "https://cdn.betterttv.net/emote/566ca06065dbbdab32ec054e/3x",
    "https://cdn.betterttv.net/emote/56901914991f200c34ffa656/3x",
    "https://cdn.betterttv.net/emote/56e9f494fff3cc5c35e5287e/3x",
    "https://cdn.betterttv.net/emote/567b00c61ddbe1786688a633/3x",
    "https://cdn.betterttv.net/emote/56d937f7216793c63ec140cb/3x",
    "https://cdn.betterttv.net/emote/566c9ff365dbbdab32ec0541/3x",
    "https://cdn.betterttv.net/emote/5709ab688eff3b595e93c595/3x",
    "https://cdn.betterttv.net/emote/56fa09f18eff3b595e93ac26/3x",
    "https://cdn.betterttv.net/emote/59cf182fcbe2693d59d7bf46/3x",
    "https://cdn.betterttv.net/emote/5733ff12e72c3c0814233e20/3x",
    "https://cdn.betterttv.net/emote/573d38b50ffbf6cc5cc38dc9/3x",
    "https://cdn.betterttv.net/emote/56f5be00d48006ba34f530a4/3x",
    "https://cdn.betterttv.net/emote/566ca02865dbbdab32ec0547/3x",
    "https://cdn.betterttv.net/emote/550b344bff8ecee922d2a3c1/3x",
    "https://cdn.betterttv.net/emote/58d2e73058d8950a875ad027/3x",
    "https://cdn.betterttv.net/emote/550352766f86a5b26c281ba2/3x",
    "https://cdn.betterttv.net/emote/55028cd2135896936880fdd7/3x",
    "https://cdn.betterttv.net/emote/552d2fc2236a1aa17a996c5b/3x",
    "https://cdn.betterttv.net/emote/566ca09365dbbdab32ec0555/3x",
    "https://cdn.betterttv.net/emote/566ca04265dbbdab32ec054a/3x",
    "https://cdn.betterttv.net/emote/55189a5062e6bd0027aee082/3x",
    "https://cdn.betterttv.net/emote/566c9f3b65dbbdab32ec052e/3x",
    "https://cdn.betterttv.net/emote/566c9f6365dbbdab32ec0532/3x",
    "https://cdn.betterttv.net/emote/566c9eeb65dbbdab32ec052b/3x",
    "https://cdn.betterttv.net/emote/566ca00f65dbbdab32ec0544/3x",
    "https://cdn.betterttv.net/emote/5dc36a8db537d747e37ac187/3x",
    "https://cdn.betterttv.net/emote/55b6f480e66682f576dd94f5/3x",
    "https://cdn.betterttv.net/emote/5ab0eab0b2ab933caccf41ba/3x",
    "https://cdn.betterttv.net/emote/58868aa5afc2ff756c3f495e/3x",
    "https://cdn.betterttv.net/emote/5e76d338d6581c3724c0f0b2/3x",
    "https://cdn.betterttv.net/emote/5e76d399d6581c3724c0f0b8/3x",
    "https://cdn.betterttv.net/emote/5a9578d6dcf3205f57ba294f/3x",
    "https://cdn.betterttv.net/emote/5e76d2d2d112fc372574d222/3x",
    "https://cdn.betterttv.net/emote/5e76d2ab8c0f5c3723a9a87d/3x",
    "https://cdn.betterttv.net/emote/57719a9a6bdecd592c3ad59b/3x",
    "https://cdn.betterttv.net/emote/5608cf93fdaf5f3275fe39cd/3x",
    "https://cdn.betterttv.net/emote/5b1740221c5a6065a7bad4b5/3x",
    "https://cdn.betterttv.net/emote/5b8ce8622431b408aeef113e/3x",
    "https://cdn.betterttv.net/emote/5b069731b6943931c1286e94/3x",
    "https://cdn.betterttv.net/emote/5b77ac3af7bddc567b1d5fb2/3x",
    "https://cdn.betterttv.net/emote/59420c88023a013b50774872/3x",
    "https://cdn.betterttv.net/emote/5b3e953a2c8a38720760c7f7/3x",
    "https://cdn.betterttv.net/emote/5b35cae2f3a33e2b6f0058ef/3x",
    "https://cdn.betterttv.net/emote/5b0da25016252035ad06900e/3x",
    "https://cdn.betterttv.net/emote/5befebbbe4ac1871e89f2485/3x",
    "https://cdn.betterttv.net/emote/55e2096ea6fa8b261f81b12a/3x",
    "https://cdn.betterttv.net/emote/58e5abdaf3ef4c75c9c6f0f9/3x",
    "https://cdn.betterttv.net/emote/56f6eb647ee3e8fc6e4fe48e/3x",
    "https://cdn.betterttv.net/emote/5c85dd4b2bc49a0419824494/3x",
    "https://cdn.betterttv.net/emote/5aa1d0e311237146531078b0/3x",
    "https://cdn.betterttv.net/emote/5cb7212adc35c51617f46ef4/3x",
    "https://cdn.betterttv.net/emote/5a7fd054b694db72eac253f4/3x",
    "https://cdn.betterttv.net/emote/59a2a35339266478fce92509/3x",
    "https://cdn.betterttv.net/emote/5bc7ff14664a3b079648dd66/3x",
    "https://cdn.betterttv.net/emote/5a546b50b09c687f988a41aa/3x",
    "https://cdn.betterttv.net/emote/571e5ae5ef2b92040a9682c7/3x",
    "https://cdn.betterttv.net/emote/5af84b9e766af63db43bf6b9/3x",
    "https://cdn.betterttv.net/emote/5d0ae0d1aae4851c803d4710/3x",
    "https://cdn.betterttv.net/emote/5da63a2c3a6a014ac8f89781/3x",
    "https://cdn.betterttv.net/emote/5c7e79ce298047130f9fb76f/3x",
    "https://cdn.betterttv.net/emote/5e1a67c6bca2995f13fb4478/3x",
    "https://cdn.betterttv.net/emote/56c2cff2d9ec6bf744247bf1/3x",
    "https://cdn.betterttv.net/emote/5afe99e57724ae2706b27355/3x",
    "https://cdn.betterttv.net/emote/576befd71f520d6039622f7e/3x",
    "https://cdn.betterttv.net/emote/5b5c15e86356197a5926fefd/3x",
    "https://cdn.betterttv.net/emote/56cb56f5500cb4cf51e25b90/3x",
    "https://cdn.betterttv.net/emote/5bc2143ea5351f40921080ee/3x",
    "https://cdn.betterttv.net/emote/5d11513d98ad5d5a53c59c2e/3x",
    "https://cdn.betterttv.net/emote/5ad22a7096065b6c6bddf7f3/3x",
    "https://cdn.betterttv.net/emote/5e7009f8d6581c3724c093c0/3x",
    "https://cdn.betterttv.net/emote/5e703cc0d112fc372574789d/3x",
    "https://cdn.betterttv.net/emote/5bba7b0f9318d3172d079f0a/3x",
    "https://cdn.betterttv.net/emote/5e0ab5918245800d975659b3/3x",
    "https://cdn.betterttv.net/emote/5b6bc260e94fb54c74b60002/3x",
    "https://cdn.betterttv.net/emote/5c447084f13c030e98f74f58/3x",
    "https://cdn.betterttv.net/emote/5b1daf38ef4acb2fc030a94d/3x",
    "https://cdn.betterttv.net/emote/5cc4f582cdc967059a17a70c/3x",
    "https://cdn.betterttv.net/emote/59143b496996b360ff9b807c/3x",
    "https://cdn.betterttv.net/emote/5674bb22bf317838643c7de9/3x",
    "https://cdn.betterttv.net/emote/5823a0167df8f1a41d082cf2/3x",
    "https://cdn.betterttv.net/emote/566ca38765dbbdab32ec0560/3x",
};
#define MAX_PARALLEL 20
#define NUM_URLS sizeof(urls)/sizeof(char *)

static size_t write_cb(char *data, size_t n, size_t l, void *userp)
{
    /* take care of the data here, ignored in this example */
    (void)data;
    (void)userp;
    return n*l;
}

static void add_transfer(CURLM *cm, int i)
{
    CURL *eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(eh, CURLOPT_URL, urls[i]);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, urls[i]);
    curl_multi_add_handle(cm, eh);
}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    CURLM *cm = curl_multi_init();

    curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, (long) MAX_PARALLEL);

    unsigned int transfers = 0;
    for (transfers = 0; transfers < MAX_PARALLEL; ++transfers) {
        add_transfer(cm, transfers);
    }

    int still_alive = 1;
    do {
        curl_multi_perform(cm, &still_alive);

        CURLMsg *msg;
        int msgs_left = -1;
        while((msg = curl_multi_info_read(cm, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                char *url = NULL;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
                fprintf(stderr, "R: %d - %s <%s>\n",
                        msg->data.result, curl_easy_strerror(msg->data.result), url);

                CURL *e = msg->easy_handle;
                curl_multi_remove_handle(cm, e);
                curl_easy_cleanup(e);
            } else {
                fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
            }

            if (transfers < NUM_URLS) {
                add_transfer(cm, transfers++);
            }

            if(still_alive) curl_multi_wait(cm, NULL, 0, 1000, NULL);
        }
    } while (still_alive || (transfers < NUM_URLS));

    curl_multi_cleanup(cm);
    curl_global_cleanup();

    return EXIT_SUCCESS;
}
