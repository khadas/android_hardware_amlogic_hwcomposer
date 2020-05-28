#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

change_mode() {
	cycle=0
	while true;do
		list=`meson_display_client -l|cut -d' ' -f1`;
		let cycle++;

		for mode in $list;do
			echo -en "\rchange mode[$cycle] $(meson_display_client -G display-mode) =>$mode";
			meson_display_client -c $mode;
			sleep 2;
		done
	done
}


window_w=1920;
window_h=1080;
getPosition() {
	rate=$1
	((rate=rate+90));
	((rw=window_w*rate/100));
	((rh=window_h*rate/100));
	((rx=(window_w-rw)/2));
	((ry=(window_h-rh)/2));
	echo "$rx $ry $rw $rh"
}

change_position() {
	rect=`meson_display_client -G ui-rect`;
	window_w=`echo $rect|cut -d"," -f3`
	window_w=`echo $rect|cut -d"," -f4`
	while true;do
		let cycle++;
		for mode in 1 2 3 4 5 6 7 8 9 0;do
			mode=`getPosition $mode`
			echo  "change ui-rect[$cycle] $(meson_display_client -G ui-rect) =>$mode";
			meson_display_client -S ui-rect $mode
		done
	done
}

graphic_file_list="
/sys/class/video/axis
/sys/class/video/video_layer1_state
/sys/class/graphics/fb0/blank
/sys/class/graphics/fb1/blank
/sys/class/video/device_resolution
/sys/class/graphics/fb0/free_scale
/sys/class/graphics/fb1/free_scale
/sys/class/graphics/fb0/freescale_mode
/sys/class/graphics/fb1/freescale_mode
/sys/class/graphics/fb0/scale_axis
/sys/class/graphics/fb0/free_scale_axis
/sys/class/graphics/fb0/window_axis
"

graphic_file_wathcer() {
    for file in $graphic_file_list;do
        test_watcher -F $file &
	done
    while true;do
        sleep 10;
	done
}

init() {
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib
    export PATH=$PATH:/data
    chmod 777 /data/test_* /data/meson_display_client
}

echo <<EOF
Usage: \
# you need copy this script test_watcher and meson_display_client into /data/ first
    1: set display mode
    2: set position
    3: graphic file change watcher
EOF

init;
case $1 in
   1) change_mode;;
   2) change_position;;
   3) graphic_file_wathcer;;
esac
