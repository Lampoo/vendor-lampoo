#!/bin/bash

# Parameter Substition: https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html

# ${parameter:-word}
# If parameter is unset or null, the expansion of word is substituted. Otherwise, the value of parameter is substituted. 
OVERLAY_DIR=${1:-$ANDROID_BUILD_TOP/vendor/lampoo/build/core/overlay}

# ${parameter/pattern/string}
# The pattern is expanded to produce a pattern just as in filename expansion.
PROJECT_PATH="${PWD/$ANDROID_BUILD_TOP\//}"

if [ -d $OVERLAY_DIR/$PROJECT_PATH ] ; then
	OVERLAY_PATCHES=`ls $OVERLAY_DIR/$PROJECT_PATH/*.patch`
	for f in $OVERLAY_PATCHES; do
		if git apply --check $f &> /dev/null ; then
			echo Applying `basename $f` to $PROJECT_PATH
			git am $f
		fi
	done
fi
