struct dwarfmode_hook : public df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    int get_menu_width()
    {
        uint8_t menu_width, area_map_width;
        Gui::getMenuWidth(menu_width, area_map_width);
        int32_t menu_w = 0;

        bool menuforced = (ui->main.mode != df::ui_sidebar_mode::Default || df::global::cursor->x != -30000);

        if ((menuforced || menu_width == 1) && area_map_width == 2) // Menu + area map
            menu_w = 55;
        else if (menu_width == 2 && area_map_width == 2) // Area map only
        {
            menu_w = 24;
        }
        else if (menu_width == 1) // Wide menu
            menu_w = 55;
        else if (menuforced || (menu_width == 2 && area_map_width == 3)) // Menu only
            menu_w = 31;

        return menu_w;
    }

    DEFINE_VMETHOD_INTERPOSE(void, feed, (std::set<df::interface_key> *input))
    {
        renderer_cool *r = (renderer_cool*)enabler->renderer;

        init->display.grid_x = r->gdimxfull + gmenu_w + 2;
        init->display.grid_y = r->gdimyfull + 2;

        INTERPOSE_NEXT(feed)(input);

        init->display.grid_x = tdimx;
        init->display.grid_y = tdimy;

        int menu_w_new = get_menu_width();
        if (gmenu_w != menu_w_new)
        {
            gmenu_w = menu_w_new;
            r->needs_reshape = reshape_sidebar;
        }
    }

    DEFINE_VMETHOD_INTERPOSE(void, logic, ())
    {
        INTERPOSE_NEXT(logic)();

        renderer_cool *r = (renderer_cool*)enabler->renderer;

        if (df::global::ui->follow_unit != -1)
        {
            df::unit *u = df::unit::find(df::global::ui->follow_unit);
            if (u)
            {
                *df::global::window_x = std::max(0, std::min(world->map.x_count - r->gdimxfull, u->pos.x - r->gdimx / 2));
                *df::global::window_y = std::max(0, std::min(world->map.y_count - r->gdimyfull, u->pos.y - r->gdimy / 2));
            }
        }
    }

    DEFINE_VMETHOD_INTERPOSE(void, render, ())
    {
        screen_map_type = 1;

        renderer_cool *r = (renderer_cool*)enabler->renderer;

        if (gmenu_w < 0)
        {
            gmenu_w = get_menu_width();
            r->needs_reshape = reshape_sidebar;
        }

        r->reshape_zoom_swap();

        memset(gscreen_under, 0, r->gdimx*r->gdimy*sizeof(uint32_t));
        screen_under_ptr = gscreen_under;
        screen_ptr = gscreen;
        mwindow_x = gwindow_x;

        INTERPOSE_NEXT(render)();

#ifdef WIN32
        static bool patched = false;
        if (!patched)
        {
            MemoryPatcher p(Core::getInstance().p.get());
            apply_patch(&p, p_dwarfmode_render);
            patched = true;
        }
#endif

        // These values may change from the main thread while being accessed from the rendering thread,
        // and that will cause flickering of overridden tiles at least, so save them here
        gwindow_x = *df::global::window_x;
        gwindow_y = *df::global::window_y;
        gwindow_z = *df::global::window_z;

        uint32_t *z = (uint32_t*)gscreen;
        for (int y = 0; y < r->gdimy; y++)
        {
            for (int x = world->map.x_count-*df::global::window_x; x < r->gdimx; x++)
            {
                z[x*r->gdimy+y] = 0;
                gscreentexpos[x*r->gdimy+y] = 0;
            }
        }
        for (int x = 0; x < r->gdimx; x++)
        {
            for (int y = world->map.y_count-*df::global::window_y; y < r->gdimy; y++)
            {
                z[x*r->gdimy+y] = 0;
                gscreentexpos[x*r->gdimy+y] = 0;
            }
        }

        uint8_t *sctop                     = gps->screen;
        long *screentexpostop              = gps->screentexpos;
        int8_t *screentexpos_addcolortop   = gps->screentexpos_addcolor;
        uint8_t *screentexpos_grayscaletop = gps->screentexpos_grayscale;
        uint8_t *screentexpos_cftop        = gps->screentexpos_cf;
        uint8_t *screentexpos_cbrtop       = gps->screentexpos_cbr;

        // In fort mode render_map() will render starting at (1,1)
        // and will use dimensions from init->display.grid to calculate map region to render
        // but dimensions from gps to calculate offsets into screen buffer.
        // So we adjust all this so that it renders to our gdimx x gdimy buffer starting at (0,0).
        gps->screen                 = gscreen                 - 4*r->gdimy - 4;
        gps->screen_limit           = gscreen                 + r->gdimx * r->gdimy * 4;
        gps->screentexpos           = gscreentexpos           - r->gdimy - 1;
        gps->screentexpos_addcolor  = gscreentexpos_addcolor  - r->gdimy - 1;
        gps->screentexpos_grayscale = gscreentexpos_grayscale - r->gdimy - 1;
        gps->screentexpos_cf        = gscreentexpos_cf        - r->gdimy - 1;
        gps->screentexpos_cbr       = gscreentexpos_cbr       - r->gdimy - 1;

        init->display.grid_x = r->gdimx + gmenu_w + 2;
        init->display.grid_y = r->gdimy + 2;
        gps->dimx = r->gdimx;
        gps->dimy = r->gdimy;
        gps->clipx[1] = r->gdimx;
        gps->clipy[1] = r->gdimy;

        if (maxlevels)
            patch_rendering(false);

        render_map();

        if (maxlevels)
        {
            multi_rendered = false;

            gps->screen                 = mscreen                 - 4*r->gdimy - 4;
            gps->screen_limit           = mscreen                 + r->gdimx * r->gdimy * 4;
            gps->screentexpos           = mscreentexpos           - r->gdimy - 1;
            gps->screentexpos_addcolor  = mscreentexpos_addcolor  - r->gdimy - 1;
            gps->screentexpos_grayscale = mscreentexpos_grayscale - r->gdimy - 1;
            gps->screentexpos_cf        = mscreentexpos_cf        - r->gdimy - 1;
            gps->screentexpos_cbr       = mscreentexpos_cbr       - r->gdimy - 1;

            screen_under_ptr = mscreen_under;
            screen_ptr = mscreen;

            bool lower_level_rendered = false;
            int p = 1;
            int x0 = 0;
            int zz0 = *df::global::window_z; // Current "top" zlevel
            int maxp = std::min(maxlevels, zz0);

            do
            {
                if (p == maxlevels)
                    patch_rendering(true);

                (*df::global::window_z)--;

                lower_level_rendered = false;
                int x00 = x0;
                int zz = zz0 - p + 1; // Last rendered zlevel in gscreen, the tiles of which we're checking below

                int x1 = std::min(r->gdimx, world->map.x_count-*df::global::window_x);
                int y1 = std::min(r->gdimy, world->map.y_count-*df::global::window_y);
                for (int x = x0; x < x1; x++)
                {
                    for (int y = 0; y < y1; y++)
                    {
                        const int tile = x * r->gdimy + y, stile = tile * 4;
                        unsigned char ch = gscreen[stile+0];

                        // Continue if the tile is not empty and doesn't look like a ramp
                        if (ch != 0 && ch != 31)
                            continue;

                        int xx = *df::global::window_x + x;
                        int yy = *df::global::window_y + y;
                        if (xx < 0 || yy < 0)
                            continue;

                        int xxquot = xx >> 4, xxrem = xx & 15;
                        int yyquot = yy >> 4, yyrem = yy & 15;

                        // If the tile looks like a ramp, check that it's really a ramp
                        // Also, no need to go deeper if the ramp is covered with water
                        if (ch == 31)
                        {
                            df::map_block *block0 = world->map.block_index[xxquot][yyquot][zz];
                            if (block0->tiletype[xxrem][yyrem] != df::tiletype::RampTop || block0->designation[xxrem][yyrem].bits.flow_size)
                                continue;
                        }

                        // If the tile is empty, render the next zlevel (if not rendered already)
                        if (!lower_level_rendered)
                        {
                            multi_rendered = true;

                            // All tiles to the left were not empty, so skip them
                            x0 = x;

                            (*df::global::window_x) += x0;
                            init->display.grid_x -= x0;
                            mwindow_x = gwindow_x + x0;

                            memset(mscreen_under, 0, (r->gdimx-x0)*r->gdimy*sizeof(uint32_t));
                            render_map();

                            (*df::global::window_x) -= x0;
                            init->display.grid_x += x0;

                            x00 = x0;

                            lower_level_rendered = true;
                        }

                        const int tile2 = (x-(x00)) * r->gdimy + y, stile2 = tile2 * 4;

                        *((uint32_t*)gscreen + tile) = *((uint32_t*)mscreen + tile2);
                        *((uint32_t*)gscreen_under + tile) = *((uint32_t*)mscreen_under + tile2);
                        if (*(mscreentexpos+tile2))
                        {
                            *(gscreentexpos + tile) = *(mscreentexpos + tile2);
                            *(gscreentexpos_addcolor + tile) = *(mscreentexpos_addcolor + tile2);
                            *(gscreentexpos_grayscale + tile) = *(mscreentexpos_grayscale + tile2);
                            *(gscreentexpos_cf + tile) = *(mscreentexpos_cf + tile2);
                            *(gscreentexpos_cbr + tile) = *(mscreentexpos_cbr + tile2);
                        }
                        gscreen[stile+3] = (0x10*p) | (gscreen[stile+3]&0x0f);
                    }
                }

                if (p++ >= maxp)
                    break;
            } while(lower_level_rendered);

            (*df::global::window_z) = zz0;
        }

        init->display.grid_x = gps->dimx = tdimx;
        init->display.grid_y = gps->dimy = tdimy;
        gps->clipx[1] = gps->dimx - 1;
        gps->clipy[1] = gps->dimy - 1;

        gps->screen                 = sctop;
        gps->screen_limit           = gps->screen + gps->dimx * gps->dimy * 4;
        gps->screentexpos           = screentexpostop;
        gps->screentexpos_addcolor  = screentexpos_addcolortop;
        gps->screentexpos_grayscale = screentexpos_grayscaletop;
        gps->screentexpos_cf        = screentexpos_cftop;
        gps->screentexpos_cbr       = screentexpos_cbrtop;

        if (block_index_size != world->map.x_count_block*world->map.y_count_block*world->map.z_count_block)
        {
            free(my_block_index);
            block_index_size = world->map.x_count_block*world->map.y_count_block*world->map.z_count_block;
            my_block_index = (df::map_block**)malloc(block_index_size*sizeof(void*));

            for (int x = 0; x < world->map.x_count_block; x++)
            {
                for (int y = 0; y < world->map.y_count_block; y++)
                {
                    for (int z = 0; z < world->map.z_count_block; z++)
                    {
                        my_block_index[x*world->map.y_count_block*world->map.z_count_block + y*world->map.z_count_block + z] = world->map.block_index[x][y][z];
                    }
                }
            }
        }
    }
};

IMPLEMENT_VMETHOD_INTERPOSE_PRIO(dwarfmode_hook, render, -200);
IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, feed);
IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, logic);
