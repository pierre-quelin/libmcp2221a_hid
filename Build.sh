#!/bin/sh

_FETCH=0
_GEN=0
_CHECK=0
_CLEAN=0
export _RECURSE=1
_PAUSE=0

end()
{
   if [ $1 -eq 1 ] ; then
      read -p "Press [Enter] key to continue..." fake
   fi
   exit $2
}

echo ${0%/*}

# ===================== Select Service =======================================
# Interactive
if [ $# -eq 0 ] ; then
{
   _PAUSE=1
   echo "--------------------"
   echo "-> Select service <-"
   echo "--------------------"
   echo "[ ] - all     (fetch + gen)"
   echo "[1] - fetch   (retrieve all dependencies)"
   echo "[2] - gen     (build the target)"
   echo "[3] - check   (run cppcheck)"
   echo "[9] - clean"
   echo
   echo "[0] - Quit"
   echo
   
   read -p "Enter your choice: " _USRINPUT
   
   case "$_USRINPUT" in
      "0")  echo "0 -> Bye"; exit0;;
      "1")  _FETCH=1; echo "fetch";;
      "2")  _GEN=1; echo "gen";;
      "3")  _CHECK=1; echo "check";;
      "9")  _CLEAN=1; echo "clean";;
      "")   _FETCH=1; _GEN=1; echo "all" ;;
      *)    echo "$_USRINPUT -> Bad choice -> Bye"; end $_PAUSE;;
   esac
}
# Not interactive
else
{
   for a in $* ; do
      case "$a" in
         "-n")    _RECURSE=0; echo "recurse";;
         "fetch") _FETCH=1; echo "fetch";;
         "gen")   _GEN=1; echo "gen";;
         "check") _CHECK=1; echo "check";;
         "clean") _CLEAN=1; echo "clean";;
         "all")   _FETCH=1; _GEN=1; echo "all" ;;
         *)       echo "$_USRINPUT -> Bad choice -> Bye"; end $_PAUSE;;
      esac
   done
}
fi

echo ${0%/*}

# ===================== Clean ================================================
if [ $_CLEAN -eq 1 ] ; then
{
   if [ -f ${0%/*}/tools/Build_Clean.sh ] ; then
   {
      ${0%/*}/tools/Build_Clean.sh
      if [ $? -eq 1 ] ; then
         echo "Build_Clean.sh failed !"
	      end $_PAUSE 1
      fi
   }
   fi
}
fi

# ===================== Fetch ================================================
if [ $_FETCH -eq 1 ] ; then
{
   if [ -f ${0%/*}/tools/Build_Fetch.sh ] ; then
   {
      ${0%/*}/tools/Build_Fetch.sh
      if [ $? -eq 1 ] ; then
         echo "Build_Fetch.sh failed !"
	      end $_PAUSE 1
      fi
   }
   fi
}
fi
 
# ===================== Gen ==================================================
if [ $_GEN -eq 1 ] ; then
{
   if [ -f ${0%/*}/tools/Build_Gen.sh ] ; then
   {
      ${0%/*}/tools/Build_Gen.sh
      if [ $? -eq 1 ] ; then
         echo "Build_Gen.sh failed !"
	      end $_PAUSE 1
      fi
   }
   fi
}
fi

# ===================== Check ================================================
if [ $_CHECK -eq 1 ] ; then
{
   if [ -f ${0%/*}/tools/Build_Check.sh ] ; then
   {
      ${0%/*}/tools/Build_Check.sh
      if [ $? -eq 1 ] ; then
         echo "Build_Check.sh failed !"
         end $_PAUSE 1
      fi
   }
   fi
}
fi

end $_PAUSE 0