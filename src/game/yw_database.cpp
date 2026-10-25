#include "game/yw_shell_menus.h"
#include "game/database_session.h"
#include "game/yw_game_ui_internal.h"
#include "locale/locale.h"
#include <algorithm>
#include <sstream>

namespace
{
std::string text(int id) { return Locale::Text::OpenUA(id); }
void fill(SDL_Surface *s,int x,int y,int w,int h,Uint32 color)
{
    if(w<=0||h<=0)return;
    SDL_Rect r{x,y,w,h};SDL_FillRect(s,&r,color);
}
void caption(NC_STACK_ypaworld *w,const std::string &s,int x,int y,int width)
{
    CmdStream out;FontUA::select_tileset(&out,15);
    FontUA::set_xpos(&out,x);FontUA::set_ypos(&out,y);
    FontUA::set_txtColor(&out,217,241,245);
    FontUA::FormateClippedText(w->_guiTiles[15],&out,s,width,' ');
    FontUA::set_end(&out);GFX::Engine.ProcessDrawSeq(out);
}
void smallCaption(DatabaseSession &session,const std::string &value,int x,int y,int width,bool fit=false)
{
    if(value.empty()||width<=0||!session.detailFont)return;
    auto found=session.captions.find(value);
    if(found==session.captions.end())
        found=session.captions.emplace(value,TTF_RenderUTF8_Blended(session.detailFont,value.c_str(),SDL_Color{217,241,245,255})).first;
    auto *text=found->second;if(!text)return;
    SDL_Rect from{0,0,std::min(width,text->w),text->h},to{x,y,from.w,from.h};
    if(fit&&text->w>width)SDL_BlitScaled(text,nullptr,GFX::Engine.Screen(),&to);
    else SDL_BlitSurface(text,&from,GFX::Engine.Screen(),&to);
}
int smallWidth(DatabaseSession &session,const std::string &value)
{
    int width=0,height=0;
    if(session.detailFont)TTF_SizeUTF8(session.detailFont,value.c_str(),&width,&height);
    return width;
}
std::vector<std::string> wrapCaption(DatabaseSession &session,const std::string &value,int width,int maxLines)
{
    std::vector<std::string> result;
    std::istringstream paragraphs(value);std::string paragraph;
    while((int)result.size()<maxLines&&std::getline(paragraphs,paragraph))
    {
        std::istringstream words(paragraph);std::string word,line;
        while(words>>word)
        {
            if(!line.empty()&&smallWidth(session,line+" "+word)>width)
            {result.push_back(line);line.clear();if((int)result.size()>=maxLines)return result;}
            while(smallWidth(session,word)>width&&word.size()>1)
            {
                size_t cut=word.size();
                do {--cut;while(cut>1&&((uint8_t)word[cut]&0xC0)==0x80)--cut;}
                while(cut>1&&smallWidth(session,word.substr(0,cut))>width);
                result.push_back(word.substr(0,cut));word.erase(0,cut);
                if((int)result.size()>=maxLines)return result;
            }
            if(!line.empty())line+=" ";line+=word;
        }
        result.push_back(line);
    }
    return result;
}
std::string metricLabel(World::Database::Metric m)
{
    using M=World::Database::Metric;
    switch(m)
    {
        case M::Attack:return "Attack Power";
        case M::Defense:return "Defense";
        case M::Health:return "Energy";
        case M::Speed:return "Speed";
        default:return "";
    }
}
void visible(NC_STACK_button *panel,int id,bool show)
{
    const int i=panel->GetIndexByID(id);
    if(i<0)return;
    auto &b=panel->field_d8[i];
    if(show)b.flags=(b.flags|NC_STACK_button::FLAG_DRAW)&~NC_STACK_button::FLAG_DISABLED;
    else b.flags=(b.flags&~NC_STACK_button::FLAG_DRAW)|NC_STACK_button::FLAG_DISABLED;
}
void blocks(SDL_Surface *s,int x,int y,int width,int line,int count)
{
    const int gap=std::max(2,line/7),size=std::max(2,std::min(line/2,(width-9*gap)/10));
    for(int i=0;i<10;++i)
    {
        fill(s,x+i*(size+gap),y,size,size,SDL_MapRGB(s->format,48,91,103));
        fill(s,x+i*(size+gap)+1,y+1,size-2,size-2,
             i<count?SDL_MapRGB(s->format,118,193,198):SDL_MapRGB(s->format,5,12,16));
    }
}
}

// Text wrapping keeps authored paragraphs and complete UTF-8 characters.
std::vector<std::string> db_wrap_text_columns(const std::string &text, int max_columns, int max_lines)
{
    std::vector<std::string> out;
    size_t paragraphStart = 0;
    if ( max_columns < 8 )
        max_columns = 8;

    auto push_word = [&](std::string word)
    {
        if ( word.empty() || (int)out.size() >= max_lines )
            return;

        while ( !word.empty() && (int)out.size() < max_lines )
        {
            if ( out.size() > paragraphStart && !out.back().empty() && out.back() != " " )
            {
                std::string candidate = out.back() + " " + word;
                if ( (int)candidate.size() <= max_columns )
                {
                    out.back() = candidate;
                    return;
                }
            }

            if ( (int)word.size() <= max_columns )
            {
                out.push_back(word);
                return;
            }

            int cut = max_columns;
            while ( cut > 1 && cut < (int)word.size() && ((uint8_t)word[cut] & 0xC0) == 0x80 )
                cut--; // avoid splitting in the middle of a UTF-8 continuation byte
            out.push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
    };

    size_t start = 0;
    while ( start <= text.size() && (int)out.size() < max_lines )
    {
        paragraphStart = out.size();
        size_t end = text.find('\n', start);
        std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
        while ( !line.empty() && (line.back() == '\r' || line.back() == '\n') )
            line.pop_back();

        if ( line.empty() )
        {
            out.push_back(" ");
        }
        else
        {
            size_t pos = 0;
            while ( pos < line.size() && (int)out.size() < max_lines )
            {
                while ( pos < line.size() && (line[pos] == ' ' || line[pos] == '\t') )
                    pos++;

                size_t wordStart = pos;
                while ( pos < line.size() && line[pos] != ' ' && line[pos] != '\t' )
                    pos++;

                push_word(line.substr(wordStart, pos - wordStart));
            }
        }

        if ( end == std::string::npos )
            break;
        start = end + 1;
    }

    return out;
}



bool NC_STACK_ypaworld::WorkshopPurchase(int id,uint64_t cost,bool host)
{
    if(!IsPlasmaCurrencyEnabled()||!_GameShell||_GameShell->UserName.empty()||
       id<0||id>=(int)_vhclProtos.size()||_vhclProtos[id].Index<0||
       _vhclProtos[id].purchase_cost<0||(uint64_t)_vhclProtos[id].purchase_cost!=cost||
       _workshopOwned.count(id)||cost>_plasmaCurrencyBank)return false;
    const auto oldBank=_plasmaCurrencyBank;
    _plasmaCurrencyBank-=cost;_workshopOwned.insert(id);
    if(!SaveSettings(_GameShell,fmt::sprintf("%s/user.txt",_GameShell->UserName),World::SDF_ALL))
    {_workshopOwned.erase(id);_plasmaCurrencyBank=oldBank;return false;}
    if(!host&&_levelInfo.UserMask)
        _vhclProtos[id].disable_enable_bitmask|=_levelInfo.UserMask;
    return true;
}

bool NC_STACK_ypaworld::WorkshopEquip(int id)
{
    if(!IsPlasmaCurrencyEnabled()||!_GameShell||_GameShell->UserName.empty()||
       !_workshopOwned.count(id)||id<0||id>=(int)_vhclProtos.size()||
       _vhclProtos[id].model_id!=BACT_TYPES_ROBO)return false;
    const int previous=_workshopHost;_workshopHost=id;
    if(SaveSettings(_GameShell,fmt::sprintf("%s/user.txt",_GameShell->UserName),World::SDF_ALL))return true;
    _workshopHost=previous;return false;
}

void UserData::ShowDatabaseMenu()
{
    if(!database_button)return;
    ReleaseDatabasePreview();db_session=std::make_shared<DatabaseSession>();
    db_session->LoadResources();db_session->LoadDefinitions(*p_YW);
    db_workshop=false;db_tab=0;db_page=0;db_selected=0;db_text_scroll=0;
    titel_button->HideScreen();EnvMode=ENVMODE_DATABASE;
    PopulateDatabasePage();database_button->ShowScreen();
}
void UserData::ShowWorkshopMenu()
{
    if(!p_YW->IsPlasmaCurrencyEnabled()||!database_button)return;
    ShowDatabaseMenu();db_session->LoadOffers(*db_session->definitions);
    db_workshop=true;EnvMode=ENVMODE_WORKSHOP;PopulateDatabasePage();
}
void UserData::CloseDatabaseMenu()
{
    ReleaseDatabasePreview();database_button->HideScreen();
    titel_button->ShowScreen();EnvMode=ENVMODE_TITLE;
}
void UserData::ReleaseDatabasePreview()
{
    db_session.reset();db_entry_tab=-1;db_entry_id=-1;
}
void UserData::PopulateDatabasePage()
{
    if(!database_button||!db_session||!db_session->definitions)return;
    const auto l=DatabaseLayout::For(*p_YW,db_workshop);
    const auto &source=*db_session->definitions;
    const auto &entries=db_session->catalog.Entries(db_tab);
    const int total=db_workshop?(int)db_session->offers.size():(int)entries.size();
    const int pageRows=l.rows;
    const int pages=std::max(1,(total+pageRows-1)/pageRows);
    db_page=std::max(0,std::min(db_page,pages-1));
    const int available=std::min(pageRows,total-db_page*pageRows);
    db_selected=std::max(0,std::min(db_selected,std::max(0,available-1)));
    const int top=0;
    for(int button:std::array<int,3>{{UIWidgets::DB_BTN_UNITS,UIWidgets::DB_BTN_BUILDINGS,DatabaseLayout::WorkshopButton}})
    {
        visible(database_button,button,true);
        NC_STACK_button::button_arg76 pos;pos.butID=button;pos.xpos=-1;pos.ypos=top;pos.width=-1;
        database_button->setXYWidth(&pos);
    }
    NC_STACK_button::button_arg76 pagePos;pagePos.butID=UIWidgets::DB_LABEL_PAGE;pagePos.xpos=-1;
    pagePos.ypos=db_workshop?2*l.line:l.line;pagePos.width=-1;database_button->setXYWidth(&pagePos);
    db_session->LoadDetailFont(std::max(8,l.detailLine-3));
    database_button->SetText(DatabaseLayout::WorkshopButton,db_workshop?"Database":"Workshop");
    visible(database_button,UIWidgets::DB_BTN_PURCHASE,db_workshop&&total>0);
    database_button->SetText(UIWidgets::DB_LABEL_PAGE,fmt::sprintf("%s  %d/%d  (%d)",
        db_workshop?"Workshop":(db_tab==0?"Units":"Buildings"),db_page+1,pages,total));
    for(int row=0;row<DatabaseLayout::MaxRows;++row)
    {
        const int index=db_page*pageRows+row;std::string label=" ";
        if(row<pageRows&&index<total)
        {
            const int id=db_workshop?db_session->offers[index].vehicle:entries[index];
            label=(row==db_selected?"> ":"  ")+(db_tab==0?source.GetVhclProtos()[id].name:source.GetBuildProtos()[id].Name);
            if(db_workshop&&p_YW->GetWorkshopOwned().count(id))label+=" *";
        }
        NC_STACK_button::button_arg76 pos;pos.butID=DatabaseLayout::ListButton+row;pos.xpos=-1;
        pos.ypos=l.listTop+row*l.line;pos.width=-1;database_button->setXYWidth(&pos);
        database_button->SetText(DatabaseLayout::ListButton+row,label);
        visible(database_button,DatabaseLayout::ListButton+row,row<pageRows&&index<total);
    }
    const int selected=db_page*pageRows+db_selected;
    const int id=total>0?(db_workshop?db_session->offers[selected].vehicle:entries[selected]):-1;
    if(db_entry_tab!=db_tab||db_entry_id!=id)
    {
        db_text_scroll=0;db_entry_tab=db_tab;db_entry_id=id;
        db_session->angle=0;db_session->rotationTime=0;
        db_session->preview.Clear();db_session->StopAudio();
        if(id>=0)
        {
            db_session->card=db_session->catalog.Describe(source,db_tab,id);
            if(db_session->loaded)db_session->preview.Select(*db_session->resources,source,db_tab,id);
            const auto *sound=db_tab==0?&source.GetVhclProtos()[id].sndFX[World::TVhclProto::SND_NORMAL]:&source.GetBuildProtos()[id].SndFX;
            db_session->SelectAudio(sound,true);
        }
        else db_session->card=World::Database::Card();
    }
    if(db_workshop&&id>=0)
    {
        const bool owned=p_YW->GetWorkshopOwned().count(id);
        const bool host=source.GetVhclProtos()[id].model_id==BACT_TYPES_ROBO;
        database_button->SetText(UIWidgets::DB_BTN_PURCHASE,owned?(host?(p_YW->GetWorkshopHost()==id?"Equipped":"Equip"):"Owned"):"Purchase");
        if(owned&&(!host||p_YW->GetWorkshopHost()==id))
        {NC_STACK_button::button_66arg b{UIWidgets::DB_BTN_PURCHASE,1};database_button->Disable(&b);}
    }
    db_session->ownedHosts.clear();
    for(int owned:p_YW->GetWorkshopOwned())
        if(owned>=0&&owned<(int)source.GetVhclProtos().size()&&source.GetVhclProtos()[owned].model_id==BACT_TYPES_ROBO)
            db_session->ownedHosts.push_back(owned);
    for(int row=0;row<3;++row)
    {
        const int index=db_session->ownedHosts.empty()?0:(db_page*3+row)%db_session->ownedHosts.size();
        const bool show=db_workshop&&row<(int)db_session->ownedHosts.size();
        visible(database_button,2040+row,show);
        if(show)
        {
            const int owned=db_session->ownedHosts[index];
            const auto &p=source.GetVhclProtos()[owned];
            database_button->SetText(2040+row,(owned==p_YW->GetWorkshopHost()?"> ":"  ")+p.name);
        }
    }
    PopulateDetailPane();
}
void UserData::PopulateDetailPane()
{
    if(!db_session)return;
    db_session->text.clear();db_session->weaponRows.clear();db_session->versusRows.clear();if(db_entry_id<0)return;
    const auto &card=db_session->card;
    std::string body;
    auto add=[&](const std::string &label,const std::string &value){body+=label+": "+value+"\n";};
    add(db_tab==0?"Vehicle":"Building",std::to_string(db_entry_id));
    if(!card.className.empty())add("Class",card.className);
    if(card.productionCost>=0)add("Production Cost",std::to_string(card.productionCost)+" Plasma");
    if(card.purchaseCost>=0)add("Purchase Cost",std::to_string(card.purchaseCost));
    if(card.guns>0)add("Guns",std::to_string(card.guns));
    if(card.healingPower>0)add("Healing Power",fmt::sprintf("%g",(double)card.healingPower));
    if(card.guardians>0)add("Guardians",std::to_string(card.guardians));
    if(card.radarPower>1)add("Radar Power","Enhanced reconnaissance");

    const auto l=DatabaseLayout::For(*p_YW,db_workshop);
    if(!card.weapons.empty())
    {
        std::string weapons;
        for(int wid:card.weapons)
        {
            if(!weapons.empty())weapons+=", ";
            weapons+=db_session->definitions->GetWeaponsProtos()[wid].name;
        }
        add("Weapon",weapons);
    }
    const int infoWidth=card.description.empty()?l.width:l.width*62/100;
    db_session->text=wrapCaption(*db_session,body,infoWidth-12,1024);
    db_text_scroll=0;
}
void UserData::HandleDatabaseInput()
{
    if(!db_session)return;
    const auto result=database_button->ProcessWidgetsEvents(Input);
    const int key=Input->KbdLastHit;
    if((result&&result.code==UIWidgets::DB_UP_BACK)||key==Input::KC_ESCAPE){CloseDatabaseMenu();return;}
    const int rows=DatabaseLayout::For(*p_YW,db_workshop).rows;bool change=false;
    if(result)
    {
        if(result.code==UIWidgets::DB_UP_UNITS||result.code==UIWidgets::DB_UP_BUILDINGS)
        {db_workshop=false;EnvMode=ENVMODE_DATABASE;db_tab=result.code==UIWidgets::DB_UP_UNITS?0:2;db_page=0;db_selected=0;change=true;}
        else if(result.code==DatabaseLayout::WorkshopEvent)
        {
            // Nel Workshop il tab diventa "Database" e riporta al database (Units).
            if(db_workshop){db_workshop=false;EnvMode=ENVMODE_DATABASE;db_tab=0;db_page=0;db_selected=0;change=true;}
            else{if(p_YW->IsPlasmaCurrencyEnabled())ShowWorkshopMenu();return;}
        }
        else if(result.code>=DatabaseLayout::ListEvent&&result.code<DatabaseLayout::ListEvent+rows)
        {db_selected=result.code-DatabaseLayout::ListEvent;change=true;}
        else if(db_workshop&&result.code>=2140&&result.code<2143&&!db_session->ownedHosts.empty())
        {
            const int row=result.code-2140;
            const int id=db_session->ownedHosts[(db_page*3+row)%db_session->ownedHosts.size()];
            db_session->transactionMessage=p_YW->WorkshopEquip(id)?"Host Station equipped":"Could not save selection";
            change=true;
        }
        else if(db_workshop&&result.code==UIWidgets::DB_UP_PURCHASE&&db_entry_id>=0)
        {
            const int id=db_entry_id;const auto &p=db_session->definitions->GetVhclProtos()[id];
            if(p_YW->GetWorkshopOwned().count(id))
                db_session->transactionMessage=p_YW->WorkshopEquip(id)?"Host Station equipped":"Could not save selection";
            else if((uint64_t)p.purchase_cost>p_YW->GetPlasmaCurrencyBank())db_session->transactionMessage="Insufficient Plasma";
            else db_session->transactionMessage=p_YW->WorkshopPurchase(id,p.purchase_cost,p.model_id==BACT_TYPES_ROBO)?"Unit unlocked":"Could not save purchase";
            change=true;
        }
    }
    const int direction=(key==Input::KC_LEFT||(result&&result.code==UIWidgets::DB_UP_PREV))?-1:
                        (key==Input::KC_RIGHT||(result&&result.code==UIWidgets::DB_UP_NEXT))?1:0;
    if(direction){db_page+=direction;db_selected=0;change=true;}
    if(key==Input::KC_UP||key==Input::KC_DOWN){db_selected+=(key==Input::KC_UP?-1:1);change=true;}
    if(change)PopulateDatabasePage();
}
void UserData::RenderDatabaseEntryMedia()
{
    if(!db_session)return;
    const auto l=DatabaseLayout::For(*p_YW,db_workshop);auto *surface=GFX::Engine.Screen();if(!surface)return;
    SDL_Rect oldClip;SDL_GetClipRect(surface,&oldClip);SDL_SetClipRect(surface,nullptr);
    const auto body=SDL_MapRGB(surface->format,5,12,16),edge=SDL_MapRGB(surface->format,48,91,103);
    const auto &card=db_session->card;
    const int modelHeight=l.previewHeight-(card.weapons.empty()?0:2*l.detailLine);
    // Opaque reading panels keep text legible over either authored background.
    const int panelTop=(db_workshop?2:1)*l.line;
    fill(surface,l.right-4,panelTop,l.width+4,l.bottom-panelTop,body);
    fill(surface,l.right,l.previewTop,l.previewWidth,l.previewHeight,edge);
    fill(surface,l.right+1,l.previewTop+1,l.previewWidth-2,l.previewHeight-2,
         db_session->preview.Available()?SDL_MapRGBA(surface->format,0,0,0,0):body);
    if(db_session->preview.Available())
    {
        const int step=std::max(0,std::min(250,(int)DTime));db_session->rotationTime+=step;
        db_session->angle=(db_session->rotationTime/40)%360;
        TF::TForm3D camera;camera.Scale=vec3d(1.0);camera.SclRot=mat3x3::Ident();camera.CalcGlobal();
        auto *previous=TF::Engine.GetViewPoint();TF::Engine.SetViewPoint(&camera);
        GFX::Engine.SetFBOBlending(1);GFX::Engine.BeginScene();
        baseRender_msg arg;arg.frameTime=step;arg.globTime=GlobalTime;arg.minZ=1;arg.maxZ=32000;
        const double fraction=std::max(0.05,std::min((double)l.previewWidth/p_YW->_screenSize.x,(double)modelHeight/p_YW->_screenSize.y));
        const double distance=db_session->preview.Radius()*1.35/fraction;
        const double x=(2.0*(l.right+l.previewWidth/2.0)/p_YW->_screenSize.x-1.0)*distance;
        const double y=(2.0*(l.previewTop+modelHeight/2.0)/p_YW->_screenSize.y-1.0)*distance;
        db_session->preview.Render(arg,vec3d(x,y,distance),db_session->angle,-18,true,true,true);
        GFX::Engine.Rasterize();GFX::Engine.EndScene();GFX::Engine.SetFBOBlending(0);TF::Engine.SetViewPoint(previous);
    }
    else caption(p_YW,text(Locale::OUA_DB_PREVIEW_UNAVAILABLE),l.right+8,l.previewTop+l.previewHeight/2,l.previewWidth-16);
    caption(p_YW,card.valid?card.name:text(Locale::OUA_DB_NO_ENTRY),l.right,2*l.line,l.previewWidth);
    if(card.valid&&card.icon&&db_session->icons&&db_session->icons->img)
    {
        const auto &font=*db_session->icons;
        SDL_Rect source=font.Glyph(card.icon);
        if(source.w>0&&source.h>0)
        {
            const int size=std::max(l.line,2*l.detailLine);
            const int width=std::max(1,size*source.w/source.h);
            SDL_Rect target{l.right+l.previewWidth-width-6,l.previewTop+6,width,size};
            SDL_BlitScaled(font.img->GetSwTex(),&source,surface,&target);
        }
    }
    int statRow=0;
    for(const auto &rating:card.ratings)
    {
        const int top=l.previewTop+statRow++*2*l.detailLine;
        if(top+2*l.detailLine>l.textTop)break;
        smallCaption(*db_session,metricLabel(rating.kind),l.statsLeft,top,l.statsWidth);
        if(rating.blocks>=0)blocks(surface,l.statsLeft,top+l.detailLine+2,l.statsWidth,l.detailLine,rating.blocks);
        else smallCaption(*db_session,fmt::sprintf("%.0f",rating.value),l.statsLeft,top+l.detailLine,l.statsWidth);
    }
    if(card.valid)
    {
        const int top=l.previewTop+statRow*2*l.detailLine;
        smallCaption(*db_session,"Score",l.statsLeft,top,l.statsWidth);
        blocks(surface,l.statsLeft,top+l.detailLine+2,l.statsWidth,l.detailLine,card.score);
    }
    fill(surface,l.right,l.textTop-3,l.width,1,edge);
    const int infoWidth=card.description.empty()?l.width:l.width*62/100;
    const int vsColumns=3,vsRows=((int)card.versus.size()+vsColumns-1)/vsColumns;
    const int vsStep=l.detailLine+10;
    const int vsTop=l.bottom-4-vsRows*vsStep;
    const int infoLines=std::max(0,(vsTop-l.textTop-4)/l.detailLine);
    for(int i=0;i<infoLines&&i<(int)db_session->text.size();++i)
    {
        const auto &line=db_session->text[i];
        smallCaption(*db_session,line,l.right+4,l.textTop+i*l.detailLine,infoWidth-8);
        if(line.find("Purchase Cost:")==0)
        {
            const int size=l.detailLine-2;
            if(auto *icon=StatusIconLoad(StatusIconPlasmaPath(p_YW),size,size))
                StatusIconRenderBitmap(p_YW,icon,l.right+8+smallWidth(*db_session,line),l.textTop+i*l.detailLine,size);
        }
    }
    for(size_t i=0;i<card.versus.size();++i)
    {
        const int width=infoWidth/vsColumns;
        const int x=l.right+4+(i%vsColumns)*width,y=vsTop+(i/vsColumns)*vsStep;
        const std::string label="VS "+(card.versus[i].first=="robo"?std::string("Host Station"):card.versus[i].first);
        smallCaption(*db_session,label,x,y,width-4,true);
        blocks(surface,x,y+l.detailLine,width-8,10,card.versus[i].second);
    }
    if(!card.description.empty())
    {
        const int x=l.right+infoWidth+8,width=l.width-infoWidth-12;
        const auto lines=wrapCaption(*db_session,"Description\n"+card.description,width,l.textLines);
        for(size_t i=0;i<lines.size();++i)
            smallCaption(*db_session,lines[i],x,l.textTop+i*l.detailLine,width);
    }
    if(!card.weapons.empty())
    {
        const int top=l.previewTop+modelHeight;
        fill(surface,l.right+1,top,l.previewWidth-2,2*l.detailLine-1,body);
        sklt_wis wire{};
        const int width=l.previewWidth/card.weapons.size();
        for(size_t i=0;i<card.weapons.size();++i)
        {
            const auto &weapon=db_session->definitions->GetWeaponsProtos()[card.weapons[i]];
            const float x=2.0f*(l.right+(i+0.5f)*width)/p_YW->_screenSize.x-1;
            const float y=2.0f*(top+l.detailLine)/p_YW->_screenSize.y-1;
            yw_RenderInfoWeaponLikeWire(p_YW,&wire,weapon.wireframe,nullptr,x,y);
        }
    }
    if(db_workshop)
    {
        p_YW->RenderPlasmaCurrencyBank();
        const int equipped=p_YW->GetWorkshopHost();
        const auto &vehicles=db_session->definitions->GetVhclProtos();
        const std::string active=equipped>=0&&equipped<(int)vehicles.size()?vehicles[equipped].name:"Mission default";
        smallCaption(*db_session,"In use: "+active,8,l.line+2,l.listWidth-16);
        // Linea di separazione sopra il blocco delle Host Station possedute.
        fill(surface,0,l.bottom-4*l.line-1,l.listWidth,l.line+1,body);
        fill(surface,0,l.bottom-4*l.line-1,l.listWidth,1,edge);
        caption(p_YW,"Owned Host Stations:",8,l.bottom-4*l.line,l.listWidth-16);
        if(!db_session->transactionMessage.empty())
            smallCaption(*db_session,db_session->transactionMessage,8,l.bottom-5*l.line,l.listWidth-16,true);
    }
    SFXEngine::SFXe.UpdateSoundCarrier(&db_session->carrier);SDL_SetClipRect(surface,&oldClip);
}
