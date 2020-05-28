#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
