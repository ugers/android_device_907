#
# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

add_lunch_combo full_907-eng
add_lunch_combo full_907-userdebug
add_lunch_combo carbon_907-eng
add_lunch_combo carbon_907-userdebug
echo ""
echo "Applying patches for AllwinnerA10"
echo ""
for p in $(find device/softwinner/907/patches/ -name "*.diff") 
	do 
		echo -n "Apply patch "$(basename $p | awk -F"." '{print $1}')
		patch -p1 < $p > /dev/null 2>&1
		if [ $? == 0 ]; then
			echo "     [DONE]"
		else
			echo "     [FAIL]"
		fi
		echo "" 
	done
echo ""
