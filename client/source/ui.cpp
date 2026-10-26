#include "ui.h"
#include "text.h"
#include "font_data.h"
#include "layout.h"
#include "soundcloud_icon.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {
constexpr uint16_t rgb(int r, int g, int b) { return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10); }
uint16_t bg = rgb(15,18,26);
uint16_t panel = rgb(26,30,41), raised = rgb(37,42,55);
constexpr auto brand = rgb(255,106,40), white = rgb(242,243,247), red = rgb(255,126,126);
uint16_t muted = rgb(148,156,176), dim = rgb(85,94,115);
uint16_t accent = brand, onAccent = bg;

unsigned glyph(uint32_t cp) { return cp >= 32 && cp < 256 ? cp - 32 : cp >= 0x400 && cp < 0x500 ? 224 + cp - 0x400 : '?' - 32; }
void clockText(char* text, std::size_t size, unsigned seconds) { std::snprintf(text,size,"%u:%02u",seconds/60,seconds%60); }
const char* statusText(PlayStatus status) {
    switch (status) {
        case PlayStatus::Ready: return "Ready to discover";
        case PlayStatus::Connecting: return "Connecting...";
        case PlayStatus::Seeking: return "Seeking...";
        case PlayStatus::Buffering: return "Buffering...";
        case PlayStatus::Playing: return "Now playing";
        case PlayStatus::Paused: return "Paused";
        case PlayStatus::Stopped: return "Stopped";
        case PlayStatus::Finished: return "Finished";
        case PlayStatus::Error: return "Let's try again";
    }
    return "";
}
void pill(Canvas& c,int x,int y,int w,const char* text,bool selected=false,int height=23) {
    c.round(x,y,w,height,selected?accent:raised);
    c.centerText(x,y,w,height,text,selected?onAccent:white);
}
uint16_t blend(uint16_t base,uint16_t tint,unsigned amount) {
    unsigned out=0x8000;
    for(unsigned shift=0;shift<15;shift+=5)
        out|=((((base>>shift)&31)*(32-amount)+((tint>>shift)&31)*amount)/32)<<shift;
    return static_cast<uint16_t>(out);
}
uint16_t shade(uint16_t color,unsigned amount,unsigned floor) {
    unsigned out=0x8000;
    for(unsigned shift=0;shift<15;shift+=5)
        out|=std::min(31u,floor+(((color>>shift)&31)*amount+16)/32)<<shift;
    return static_cast<uint16_t>(out);
}
void settingsIcon(Canvas& c) {
    // Explicit symmetric pixel mask avoids overlapping circles/spokes.
    static const char* mask[]={
        "......###......", "......###......", "..##.#####.##..",
        "..###########..", "...#########...", "..####...####..",
        "#####.....#####", "#####.....#####", "#####.....#####",
        "..####...####..", "...#########...", "..###########..",
        "..##.#####.##..", "......###......", "......###......"};
    for(int y=0;y<15;++y)for(int x=0;x<15;++x)if(mask[y][x]=='#')c.rect(225+x,6+y,1,1,accent);
}
void cloudIcon(Canvas& c) {
    for(int y=0;y<18;++y)for(int x=0;x<36;++x){
        unsigned coverage=kSoundCloudCoverage[y][x];
        if(coverage)c.rect(12+x,7+y,1,1,blend(panel,accent,coverage));
    }
}
void tabs(Canvas& c,Page page) {
    c.rect(0,0,256,27,panel);
    c.centerText(0,0,104,26,"Search",page==Page::Browse?white:muted);
    c.centerText(104,0,104,26,"Player",page==Page::Player?white:muted);
    c.round(page==Page::Browse?30:134,24,44,2,accent);
    settingsIcon(c);
}
int wrappedRows(const char* text,int width,int limit) {
    int count=0;
    while(*text && count<limit){
        const char* start=text;const char* space=nullptr;int length=0;
        while(*text && *text!='\n' && length<width/6){if(*text==' ')space=text;nextCodepoint(text);++length;}
        if(*text && *text!='\n' && space && space>start)text=space;
        ++count;if(*text=='\n')++text;while(*text==' ')++text;
    }
    return std::max(1,count);
}
void topScreen(Canvas& c,const UiModel& m) {
    c.clear(bg);
    // Audio-driven time-domain envelope, gently tinted behind the content.
    const auto visual=shade(accent,9,1);
    for(int i=0;i<32;++i){
        int level=m.visualLevels?m.visualLevels[i]:0;
        int height=level*105/255;
        c.round(i*8+1,192-height,6,height,visual);
    }
    c.rect(0,0,256,30,panel); cloudIcon(c);
    c.text(53,11,"soundcloud",white); c.text(119,11,"DS",accent);
    c.circle(197,16,2,m.wifiPhase==2?accent:dim);
    c.text(204,11,m.wifiPhase==2?"Online":"Offline",muted,48);
    c.centerText(12,48,232,18,statusText(m.status),m.status==PlayStatus::Error?red:accent);
    c.round(12,Layout::coverY,64,64,panel);
    if(m.cover){
        for(int y=0;y<64;++y)for(int x=0;x<64;++x){
            if(((y==0||y==63)&&(x<2||x>61))||((y==1||y==62)&&(x==0||x==63)))continue;
            c.pixels[(Layout::coverY+y)*256+12+x]=m.cover[y*64+x];
        }
    } else for(int i=0;i<10;++i){
        int height=11+((i*17+23)%35);
        c.round(20+i*5,Layout::trackCenterY-height/2,3,height,accent);
    }
    const char* heading=*m.title?m.title:"Your next favorite track.";
    const char* author=*m.title?m.artist:"Find it on SoundCloud.";
    int titleHeight=wrappedRows(heading,156,3)*12,artistHeight=wrappedRows(author,156,2)*12;
    int top=Layout::trackCenterY-(titleHeight+artistHeight+6)/2;
    c.centerText(88,top,156,titleHeight,heading,white,3);
    c.centerText(88,top+titleHeight+6,156,artistHeight,author,muted,2);
    if(m.status==PlayStatus::Error)c.centerText(12,148,232,22,m.message,red,2);
    if(m.diagnostics && m.status!=PlayStatus::Error){
        char label[64];std::snprintf(label,sizeof(label),"Buf %u.%us  Net %u.%uK/s  Gaps %u",
            m.bufferMs/1000,(m.bufferMs%1000)/100,m.wireBytesPerSecond/1000,
            (m.wireBytesPerSecond%1000)/100,m.underruns);
        c.centerText(12,148,232,16,label,muted);
    }
    if(m.showVolume){
        char label[24];std::snprintf(label,sizeof(label),"Volume %d%%",m.volume*100/127);
        c.round(68,166,120,22,panel);
        c.centerText(68,166,120,22,label,white);
    }
}
void wifiScreen(Canvas& c,const UiModel& m) {
    c.clear(bg); c.rect(0,0,256,27,panel);
    c.centerText(8,0,200,27,"Let's get connected",white); settingsIcon(c);
    if (m.wifiPhase==1) {
        c.centerText(12,49,232,23,"Joining your Wi-Fi...",white);
        c.centerText(12,74,232,20,"This may take a moment.",muted);
        pill(c,12,146,232,"B  Cancel"); return;
    }
    char scanLabel[40];
    if(m.networkTotal)std::snprintf(scanLabel,sizeof(scanLabel),"Networks %u-%u / %u",m.networkFirst+1,m.networkFirst+m.networkCount,m.networkTotal);
    else std::snprintf(scanLabel,sizeof(scanLabel),"Scanning...");
    c.centerText(8,29,172,17,scanLabel,muted);
    c.round(188,29,60,17,raised);c.centerText(188,29,60,17,"Rescan",white);
    for (unsigned i=0;i<m.networkCount;++i) {
        bool selected=i==m.networkSelected; int y=48+i*23;
        c.round(8,y,240,21,selected?raised:panel);
        if (selected) c.rect(8,y+3,2,15,accent);
        c.text(16,y+5,m.networks[i],selected?white:muted,166);
        c.centerText(186,y,56,21,m.networkSecurity[i],m.networkCompatible[i]?accent:red);
    }
    if (!m.networkCount) c.centerText(12,56,232,32,"Scanning for networks...",white);
    if(m.networkTotal>5){
        c.rect(251,48,2,115,raised);
        int size=std::max(6u,115u*m.networkCount/m.networkTotal);
        int offset=(115-size)*m.networkFirst/(m.networkTotal-m.networkCount);
        c.rect(251,48+offset,2,size,accent);
    }
    c.centerText(8,165,240,15,m.message,muted);
    c.centerText(8,180,240,12,"A Join  Y Key  X Saved  B Scan",dim);
}
void wifiWarningScreen(Canvas& c,const UiModel& m){
    c.clear(bg);c.rect(0,0,256,27,panel);
    c.centerText(8,0,192,27,"Unsupported network",white);
    c.centerText(200,0,56,27,"Back",muted);
    c.centerText(8,34,240,24,m.networkName,white);
    c.centerText(8,64,240,48,m.wifiWarning,red,4);
    pill(c,8,123,240,"A  Try anyway",false,27);
    pill(c,8,159,240,"B  Cancel");
}
void scrollingText(Canvas& c,int x,int y,int width,const char* value,uint16_t color,unsigned frame){
    int travel=std::max(0,static_cast<int>(codepointCount(value))*6-width);
    int offset=travel?std::clamp((static_cast<int>(frame%(travel*3+120))-60)/3,0,travel):0;
    int left=x-offset;
    while(*value && left<x+width){
        unsigned index=glyph(nextCodepoint(value));
        if(left+6>x)for(int gy=0;gy<11;++gy)for(int gx=0;gx<6;++gx)
            if(left+gx>=x && left+gx<x+width && (kFont[index][gy]&(0x80>>gx)))c.rect(left+gx,y+gy,1,1,color);
        left+=6;
    }
}
void miniPlayer(Canvas& c,const UiModel& m) {
    c.rect(0,Layout::miniTop,256,Layout::miniHeight,panel);
    c.play(19,183,accent,m.status==PlayStatus::Playing,10);
    scrollingText(c,38,177,180,*m.title?m.title:"Play default track",white,m.titleFrame);
    c.rect(235,179,8,8,accent);
}
void browse(Canvas& c,const UiModel& m) {
    c.clear(bg); tabs(c,Page::Browse);
    c.round(8,Layout::searchTop,240,Layout::searchHeight,panel);
    c.circle(21,40,4,accent); c.circle(21,40,2,panel); c.line(24,43,28,47,accent);
    c.text(38,35,*m.query?m.query:"Track, artist, something new...",*m.query?white:muted,201);
    const auto& results=*m.results;
    if (m.searching) {
        c.centerText(12,73,232,21,"Searching SoundCloud...",white);
        c.centerText(12,96,232,21,"Your music keeps playing.",muted);
        c.round(12,125,232,3,raised); c.round(12+(m.frame/2)%181,125,50,3,accent);
    } else if (*m.searchMessage) {
        c.centerText(12,68,232,43,m.searchMessage,red,3);
        c.centerText(12,119,232,22,"Tap the search field to retry.",muted);
    } else if (!m.searched) {
        c.centerText(12,80,232,36,"What would you like\nto listen to today?",white,2);
        c.centerText(12,125,232,18,"Search for a track or artist.",muted);
    } else if (!results.count) {
        c.centerText(12,74,232,25,"No playable tracks found.",white);
        c.centerText(12,99,232,25,"Try another artist or title.",muted);
    } else for (unsigned row=0;row<kSearchPageSize;++row) {
        unsigned index=m.first+row; if (index>=results.count) break;
        const auto& item=results.tracks[index]; int y=Layout::resultsTop+row*Layout::rowStride;
        bool selected=static_cast<int>(index)==m.selected;
        c.round(8,y,240,Layout::rowHeight,selected?raised:panel);
        if (selected) c.rect(8,y+3,2,17,accent);
        c.text(16,y,item.title,white,184); c.text(16,y+11,item.artist,muted,184);
        char duration[24]; clockText(duration,sizeof(duration),item.durationMs/1000);
        c.text(242-std::strlen(duration)*6,y+6,duration,std::strcmp(item.id,m.trackId)==0?accent:muted);
    }
    if (m.searched&&!m.searching) {
        char label[40]; std::snprintf(label,sizeof(label),"Page %u / %u tracks",results.offset/kSearchPageSize+1,results.count);
        c.centerText(58,Layout::pagesTop,140,Layout::pagesHeight,label,muted);
        c.round(8,Layout::pagesTop,46,Layout::pagesHeight,panel);
        c.round(202,Layout::pagesTop,46,Layout::pagesHeight,panel);
        c.centerText(8,Layout::pagesTop,46,Layout::pagesHeight,"< Prev",results.offset?white:dim);
        c.centerText(202,Layout::pagesTop,46,Layout::pagesHeight,"Next >",results.hasMore?white:dim);
    }
    miniPlayer(c,m);
}
void keyboard(Canvas& c,const UiModel& m) {
    c.clear(bg); c.rect(0,0,256,27,panel);
    if(m.editingServer)c.centerText(8,0,192,27,m.editorError?"HTTP host[:port]":"Server address",m.editorError?red:white);
    if(m.editingWifi){
        c.centerText(8,0,144,27,m.passwordLabel,m.editorError?red:white);
        c.centerText(152,0,48,27,m.passwordVisible?"Hide":"Show",accent);
    }
    c.centerText(204,0,48,27,"Back",muted);
    c.round(8,31,240,24,panel);
    const char* text=m.query; while (codepointCount(text)>36) nextCodepoint(text);
    c.text(15,37,*text?text:m.editingWifi?"Enter network password...":m.editingServer?"host:80 or IPv4:8000":"Track or artist...",*text?white:dim,222);
    if ((m.frame/30)%2==0) c.rect(15+std::min(36u,codepointCount(text))*6,37,1,11,accent);
    KeyboardKey keys[48]; unsigned count=makeKeyboard(keys,m.russianKeyboard,m.numberKeyboard,m.editingServer,m.editingWifi,m.passwordShifted,m.passwordExtras);
    for (unsigned i=0;i<count;++i) {
        const auto& key=keys[i]; bool submit=key.action==KeyAction::Search, selected=static_cast<int>(i)==m.keyboardSelected;
        c.round(key.x,key.y,key.w,key.h,submit?(*m.query?accent:raised):selected?raised:panel);
        if (selected&&!submit) c.rect(key.x+3,key.y+key.h-2,key.w-6,1,accent);
        c.centerText(key.x,key.y,key.w,key.h,key.label,submit?(*m.query?onAccent:dim):white);
    }
}
void player(Canvas& c,const UiModel& m) {
    c.clear(bg); tabs(c,Page::Player);
    char time[24]; clockText(time,sizeof(time),m.elapsed); c.text(16,42,time,white);
    clockText(time,sizeof(time),m.duration); c.text(240-std::strlen(time)*6,42,time,muted);
    if (m.scrubbing) c.centerText(64,39,128,19,"Release to seek",accent);
    else if (m.status==PlayStatus::Seeking) c.centerText(64,39,128,19,"Seeking...",muted);
    unsigned progress=m.durationMs?std::min(224u,static_cast<unsigned>(static_cast<uint64_t>(m.elapsedMs)*224/m.durationMs)):0;
    c.round(16,Layout::progressY,224,4,raised); c.round(16,Layout::progressY,progress,4,accent);
    c.circle(16+progress,Layout::progressY+2,m.scrubbing?5:3,m.durationMs?accent:dim);
    c.circle(128,Layout::playY,28,accent); c.play(128,Layout::playY,onAccent,m.status==PlayStatus::Playing,18);
    c.round(24,99,44,28,panel); c.round(188,99,44,28,panel);
    c.centerText(24,99,44,28,"-5s",m.durationMs?accent:dim);
    c.centerText(188,99,44,28,"+5s",m.durationMs?accent:dim);
    pill(c,8,Layout::trackTop,116,"Previous",false,Layout::trackHeight);
    pill(c,132,Layout::trackTop,116,"Next",false,Layout::trackHeight);
}
void settings(Canvas& c,const UiModel& m) {
    c.clear(bg); c.rect(0,0,256,27,panel);
    c.centerText(8,0,48,27,"Back",muted); c.centerText(64,0,128,27,"Settings",white);
    c.centerText(8,32,240,15,"Server / host:port",muted);
    c.round(8,Layout::serverTop,240,Layout::serverHeight,m.settingSelected==0?raised:panel);
    if(codepointCount(m.server)*6<=216)c.centerText(8,Layout::serverTop,240,Layout::serverHeight,m.server,white);
    else scrollingText(c,20,Layout::serverTop+(Layout::serverHeight-11)/2,216,m.server,white,m.serverFrame);
    pill(c,8,Layout::modeTop,116,"Emulator",m.emulatorMode,Layout::modeHeight);
    pill(c,132,Layout::modeTop,116,"Console",!m.emulatorMode,Layout::modeHeight);
    c.round(8,Layout::wifiTop,240,Layout::wifiHeight,m.settingSelected==2?raised:panel);
    c.centerText(8,112,240,14,"Wi-Fi",white);
    c.centerText(8,127,240,14,m.wifiPhase==2?(*m.networkName?m.networkName:"Connected - tap to change"):"Choose a network",muted);
    // Keep the original spacing; status shares the test button, leaving a
    // dedicated footer for the contact without moving the controls upward.
    const char* testLabel=m.settingsBusy?"Testing connection...":*m.settingsMessage?m.settingsMessage:"Test connection";
    bool selected=m.settingSelected==3;
    c.round(8,Layout::testTop,240,Layout::testHeight,selected?accent:raised);
    c.centerText(8,Layout::testTop,240,Layout::testHeight,testLabel,selected?onAccent:m.settingsError?red:white);
    c.centerText(8,178,240,12,"https://t.me/lovelyperv",accent);
}
}

void Canvas::clear(uint16_t color) { std::fill(pixels,pixels+256*192,color); }
void Canvas::rect(int x,int y,int w,int h,uint16_t color) {
    int right=std::min(256,x+w),bottom=std::min(192,y+h);
    for (int row=std::max(0,y);row<bottom;++row) for (int col=std::max(0,x);col<right;++col) pixels[row*256+col]=color;
}
void Canvas::round(int x,int y,int w,int h,uint16_t color) {
    if (w<=0||h<=0) return;
    if (w<5||h<5) { rect(x,y,w,h,color); return; }
    rect(x+2,y,w-4,h,color); rect(x,y+2,w,h-4,color); rect(x+1,y+1,w-2,h-2,color);
}
void Canvas::circle(int x,int y,int radius,uint16_t color) {
    for (int row=-radius;row<=radius;++row) for (int col=-radius;col<=radius;++col)
        if (row*row+col*col<=radius*radius) rect(x+col,y+row,1,1,color);
}
void Canvas::line(int x1,int y1,int x2,int y2,uint16_t color) {
    int dx=std::abs(x2-x1),sx=x1<x2?1:-1,dy=-std::abs(y2-y1),sy=y1<y2?1:-1,error=dx+dy;
    for (;;) { rect(x1,y1,1,1,color); if(x1==x2&&y1==y2)break; int twice=error*2;
        if(twice>=dy){error+=dy;x1+=sx;} if(twice<=dx){error+=dx;y1+=sy;} }
}
void Canvas::text(int x,int y,const char* value,uint16_t color,int width,int lines,int scale) {
    int columns=width/(6*scale),column=0,row=0; if(columns<1||lines<1)return;
    while(*value&&row<lines) {
        uint32_t cp=nextCodepoint(value); if(cp=='\n'){++row;column=0;continue;}
        if(column==columns){++row;column=0;} if(row==lines)break;
        unsigned index=glyph(cp);
        for(int gy=0;gy<11;++gy)for(int gx=0;gx<6;++gx)
            if(kFont[index][gy]&(0x80>>gx))rect(x+(column*6+gx)*scale,y+(row*12+gy)*scale,scale,scale,color);
        ++column;
    }
}
void Canvas::centerText(int x,int y,int w,int h,const char* value,uint16_t color,int lines) {
    struct Line { char text[256]{}; int minX=999,maxX=-1,minY=11,maxY=-1; } rows[4];
    int columns=w/6,count=0,minY=999,maxY=-1;
    if(columns<1)return;
    const char* next=value;
    while(*next&&count<std::min(4,lines)) {
        const char* start=next; const char* space=nullptr; int length=0;
        while(*next&&*next!='\n'&&length<columns){if(*next==' ')space=next;nextCodepoint(next);++length;}
        const char* end=next;
        if(*next&&*next!='\n'&&space&&space>start){end=space;next=space;}
        while(end>start&&end[-1]==' ')--end;
        auto& line=rows[count]; std::size_t bytes=std::min<std::size_t>(end-start,sizeof(line.text)-1);
        std::memcpy(line.text,start,bytes); line.text[bytes]=0;
        const char* p=line.text; int col=0;
        while(*p){unsigned index=glyph(nextCodepoint(p));
            for(int gy=0;gy<11;++gy)for(int gx=0;gx<6;++gx)if(kFont[index][gy]&(0x80>>gx)){
                line.minX=std::min(line.minX,col*6+gx);line.maxX=std::max(line.maxX,col*6+gx);
                line.minY=std::min(line.minY,gy);line.maxY=std::max(line.maxY,gy);}
            ++col;
        }
        if(line.maxY>=0){minY=std::min(minY,count*12+line.minY);maxY=std::max(maxY,count*12+line.maxY);}
        ++count; if(*next=='\n')++next; while(*next==' ')++next;
    }
    if(maxY<0)return;
    int top=y+(h-(maxY-minY+1))/2-minY;
    for(int i=0;i<count;++i)if(rows[i].maxX>=0)
        text(x+(w-(rows[i].maxX-rows[i].minX+1))/2-rows[i].minX,top+i*12,rows[i].text,color,w+6,1);
}
void Canvas::play(int cx,int cy,uint16_t color,bool paused,int height) {
    int bar=std::max(2,height/5),gap=bar;
    if(paused){int left=cx-(bar*2+gap)/2;rect(left,cy-height/2,bar,height,color);rect(left+bar+gap,cy-height/2,bar,height,color);return;}
    int width=height*2/3,left=cx-width/3;
    for(int row=0;row<height;++row){int distance=std::abs(2*row-(height-1));int amount=std::max(1,width*(height-1-distance)/(height-1));rect(left,cy-height/2+row,amount,1,color);}
}
void drawUi(Canvas& top,Canvas& bottom,const UiModel& model) {
    accent=model.accent; onAccent=model.onAccent;
    // All surfaces use the same hue. Mixing with the old blue base made
    // weak red/orange accents turn greenish after RGB555 quantization.
    bg=shade(accent,3,1); panel=shade(accent,5,1); raised=shade(accent,8,1);
    muted=blend(rgb(176,176,176),accent,5); dim=blend(rgb(112,112,112),accent,5);
    topScreen(top,model);
    if(model.page==Page::Settings)settings(bottom,model);
    else if(model.page==Page::WifiWarning)wifiWarningScreen(bottom,model);
    else if(model.page==Page::Keyboard)keyboard(bottom,model);
    else if(model.wifiPhase!=2)wifiScreen(bottom,model);
    else if(model.page==Page::Player)player(bottom,model);
    else browse(bottom,model);
}
