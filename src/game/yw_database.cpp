#include "game/yw_shell_menus.h"
#include "game/database_session.h"
#include "game/yw_game_ui_internal.h"
#include "locale/locale.h"
#include <algorithm>

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
    const auto l=DatabaseLayout::For(*p_YW);
    const auto &source=*db_session->definitions;
    const auto &entries=db_session->catalog.Entries(db_tab);
    const int total=db_workshop?(int)db_session->offers.size():(int)entries.size();
    const int pageRows=db_workshop?std::max(1,l.rows-5):l.rows;
    const int pages=std::max(1,(total+pageRows-1)/pageRows);
    db_page=std::max(0,std::min(db_page,pages-1));
    const int available=std::min(pageRows,total-db_page*pageRows);
    db_selected=std::max(0,std::min(db_selected,std::max(0,available-1)));
    visible(database_button,UIWidgets::DB_BTN_UNITS,!db_workshop);
    visible(database_button,UIWidgets::DB_BTN_BUILDINGS,!db_workshop);
    visible(database_button,UIWidgets::DB_BTN_PURCHASE,db_workshop&&total>0);
    database_button->SetText(UIWidgets::DB_LABEL_PAGE,fmt::sprintf("%s  %d/%d  (%d)",
        db_workshop?"Workshop":(db_tab==0?"Units":"Buildings"),db_page+1,pages,total));
    for(int row=0;row<l.rows;++row)
    {
        const int index=db_page*pageRows+row;std::string label=" ";
        if(row<pageRows&&index<total)
        {
            const int id=db_workshop?db_session->offers[index].vehicle:entries[index];
            label=(row==db_selected?"> ":"  ")+(db_tab==0?source.GetVhclProtos()[id].name:source.GetBuildProtos()[id].Name);
            if(db_workshop&&p_YW->GetWorkshopOwned().count(id))label+=" *";
        }
        database_button->SetText(UIWidgets::DB_LINE_0+row,label);
        visible(database_button,UIWidgets::DB_LINE_0+row,row<pageRows&&index<total);
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
            database_button->SetText(2040+row,(owned==p_YW->GetWorkshopHost()?"> ":"  ")+p.name+"  "+std::to_string(std::max<int64_t>(0,p.purchase_cost)));
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
    if(card.purchaseCost>=0)add("Purchase Cost",std::to_string(card.purchaseCost)+" Plasma");
    if(card.guns>0)add("Guns",std::to_string(card.guns));
    if(card.healingPower>0)add("Healing Power",fmt::sprintf("%g",(double)card.healingPower));
    if(card.guardians>0)add("Guardians",std::to_string(card.guardians));
    if(card.radarPower>1)add("Radar Power","Enhanced reconnaissance");

    const auto l=DatabaseLayout::For(*p_YW);
    const int columns=std::max(12,(l.width-12)/std::max(1,GFX::Engine.MeasureScreenTextWidth("M")));
    db_session->text=db_wrap_text_columns(body,columns,1024);
    for(const auto &versus:card.versus)
    {
        db_session->text.push_back("VS "+versus.first);
        db_session->versusRows.push_back({(int)db_session->text.size(),versus.second});
        db_session->text.push_back(" ");
    }
    for(int wid:card.weapons)
    {
        const auto &weapon=db_session->definitions->GetWeaponsProtos()[wid];
        db_session->text.push_back("Weapon: "+weapon.name);
        if(weapon.wireframe)
        {
            db_session->weaponRows.push_back({(int)db_session->text.size(),wid});
            db_session->text.push_back(" ");db_session->text.push_back(" ");db_session->text.push_back(" ");
        }
    }
    if(!card.description.empty())
    {
        const auto description=db_wrap_text_columns("Description\n"+card.description,columns,1024);
        db_session->text.insert(db_session->text.end(),description.begin(),description.end());
    }
    db_text_scroll=std::max(0,std::min(db_text_scroll,std::max(0,(int)db_session->text.size()-l.textLines)));
}
void UserData::HandleDatabaseInput()
{
    if(!db_session)return;
    const auto result=database_button->ProcessWidgetsEvents(Input);
    const int key=Input->KbdLastHit;
    if((result&&result.code==UIWidgets::DB_UP_BACK)||key==Input::KC_ESCAPE){CloseDatabaseMenu();return;}
    const int rows=DatabaseLayout::For(*p_YW).rows;bool change=false;
    if(result)
    {
        if(!db_workshop&&(result.code==UIWidgets::DB_UP_UNITS||result.code==UIWidgets::DB_UP_BUILDINGS))
        {db_tab=result.code==UIWidgets::DB_UP_UNITS?0:2;db_page=0;db_selected=0;change=true;}
        else if(result.code>=UIWidgets::DB_UP_LINE_BASE&&result.code<UIWidgets::DB_UP_LINE_BASE+rows)
        {db_selected=result.code-UIWidgets::DB_UP_LINE_BASE;change=true;}
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
    if(key==Input::KC_PGUP||key==Input::KC_PGDOWN)
    {db_text_scroll+=(key==Input::KC_PGUP?-1:1)*std::max(1,DatabaseLayout::For(*p_YW).textLines-1);PopulateDetailPane();}
    if(change)PopulateDatabasePage();
}
void UserData::RenderDatabaseEntryMedia()
{
    if(!db_session)return;
    const auto l=DatabaseLayout::For(*p_YW);auto *surface=GFX::Engine.Screen();if(!surface)return;
    SDL_Rect oldClip;SDL_GetClipRect(surface,&oldClip);SDL_SetClipRect(surface,nullptr);
    const auto body=SDL_MapRGB(surface->format,5,12,16),edge=SDL_MapRGB(surface->format,48,91,103);
    // Opaque reading panels keep text legible over either authored background.
    fill(surface,l.right-4,l.line,l.width+4,l.bottom-l.line,body);
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
        const double fraction=std::max(0.05,std::min((double)l.previewWidth/p_YW->_screenSize.x,(double)l.previewHeight/p_YW->_screenSize.y));
        const double distance=db_session->preview.Radius()*1.35/fraction;
        const double x=(2.0*(l.right+l.previewWidth/2.0)/p_YW->_screenSize.x-1.0)*distance;
        const double y=(2.0*(l.previewTop+l.previewHeight/2.0)/p_YW->_screenSize.y-1.0)*distance;
        db_session->preview.Render(arg,vec3d(x,y,distance),db_session->angle,-18,true,true,true);
        GFX::Engine.Rasterize();GFX::Engine.EndScene();GFX::Engine.SetFBOBlending(0);TF::Engine.SetViewPoint(previous);
    }
    else caption(p_YW,text(Locale::OUA_DB_PREVIEW_UNAVAILABLE),l.right+8,l.previewTop+l.previewHeight/2,l.previewWidth-16);
    caption(p_YW,"Model",l.right,2*l.line,l.previewWidth);
    const auto &card=db_session->card;
    if(card.valid&&card.icon)
    {
        const auto *font=p_YW->_guiTiles[26];
        if(font&&font->img)
        {
            SDL_Rect source=font->Glyph(card.icon);
            if(source.w>0&&source.h>0)
            {
                SDL_Rect target{l.right,l.line,std::min(2*l.line,l.line*source.w/source.h),l.line};
                SDL_BlitScaled(font->img->GetSwTex(),&source,surface,&target);
            }
        }
    }
    caption(p_YW,card.valid?card.name:text(Locale::OUA_DB_NO_ENTRY),l.right+2*l.line,l.line,l.width-2*l.line);
    int statRow=0;
    for(const auto &rating:card.ratings)
    {
        const int top=l.previewTop+statRow++*2*l.line;
        if(top+2*l.line>l.textTop)break;
        caption(p_YW,metricLabel(rating.kind),l.statsLeft,top,l.statsWidth);
        if(rating.blocks>=0)blocks(surface,l.statsLeft,top+l.line+2,l.statsWidth,l.line,rating.blocks);
        else caption(p_YW,fmt::sprintf("%.0f",rating.value),l.statsLeft,top+l.line,l.statsWidth);
    }
    fill(surface,l.right,l.textTop-3,l.width,1,edge);
    for(int i=0;i<l.textLines&&i+db_text_scroll<(int)db_session->text.size();++i)
        caption(p_YW,db_session->text[i+db_text_scroll],l.right+4,l.textTop+i*l.line,l.width-8);
    for(const auto &row:db_session->versusRows)
    {
        const int y=row.first-db_text_scroll;
        if(y>=0&&y<l.textLines)blocks(surface,l.right+8,l.textTop+y*l.line+2,l.width,l.line,row.second);
    }
    sklt_wis wire{};
    for(const auto &row:db_session->weaponRows)
    {
        const int y=row.first-db_text_scroll;
        if(y<0||y+3>l.textLines)continue;
        const auto &weapon=db_session->definitions->GetWeaponsProtos()[row.second];
        const float x=2.0f*(l.right+l.width/2)/p_YW->_screenSize.x-1;
        const float top=2.0f*(l.textTop+(y+1.5f)*l.line)/p_YW->_screenSize.y-1;
        yw_RenderInfoWeaponLikeWire(p_YW,&wire,weapon.wireframe,nullptr,x,top);
    }
    if(db_workshop)
    {
        p_YW->RenderPlasmaCurrencyBank();
        if(db_entry_id>=0)
        {
            const auto &p=db_session->definitions->GetVhclProtos()[db_entry_id];
            const int y=l.bottom-3*l.line;
            fill(surface,0,y,l.listWidth,l.line,body);
            const int size=l.line;
            if(auto *icon=StatusIconLoad(StatusIconPlasmaPath(p_YW),size,size))
                StatusIconRenderBitmap(p_YW,icon,8,y,size);
            caption(p_YW,std::to_string(p.purchase_cost),12+size,y,l.listWidth-size-16);
        }
        const int equipped=p_YW->GetWorkshopHost();
        const auto &vehicles=db_session->definitions->GetVhclProtos();
        const std::string active=equipped>=0&&equipped<(int)vehicles.size()?vehicles[equipped].name:"Mission default";
        caption(p_YW,"In use: "+active,8,l.line,l.listWidth-16);
        fill(surface,0,l.bottom-8*l.line,l.listWidth,l.line,body);
        caption(p_YW,"Owned Host Stations",8,l.bottom-8*l.line,l.listWidth-16);
        if(!db_session->transactionMessage.empty())
            caption(p_YW,db_session->transactionMessage,l.right,l.bottom-2*l.line,l.width);
    }
    caption(p_YW,"PgUp / PgDn",l.right,l.bottom-l.line,l.width);
    SFXEngine::SFXe.UpdateSoundCarrier(&db_session->carrier);SDL_SetClipRect(surface,&oldClip);
}
