import os.path
import os

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')

    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        mock = os.environ.get('MOCK_TIME')          # e.g. MOCK_TIME=20:47 pebble build
        if mock:
            h, m = mock.split(':')[:2]
            ctx.env.append_unique('DEFINES', ['MOCK_H=%d' % int(h), 'MOCK_M=%d' % int(m)])
        anim = os.environ.get('MOCK_ANIM_MS')       # freeze the minute change at this ms
        if anim:
            ctx.env.append_unique('DEFINES', ['MOCK_ANIM_MS=%d' % int(anim)])
        daypart = os.environ.get('MOCK_DAYPART')    # 0 sunrise, 1 midday, 2 sunset, 3 night
        if daypart:
            ctx.env.append_unique('DEFINES', ['MOCK_DAYPART=%d' % int(daypart)])
        weather = os.environ.get('MOCK_WEATHER')    # WxKind: 0 clear .. 7 storm
        if weather:
            ctx.env.append_unique('DEFINES', ['MOCK_WEATHER=%d' % int(weather)])
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        # basalt draws no weather, so its heap does not pay for the weather modules
        excl = ['src/c/weather.c', 'src/c/bolt.c'] if platform == 'basalt' else []
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c', excl=excl),
                      target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
