GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'
DATE=$date

clear

echo -e "\n${BLUE} -- Compiling $(DATE) --${NC}"

make config=release -j

echo -e "\n${GREEN} -- Starting execution $(DATE) --${NC}"

./Server/bin/Server
