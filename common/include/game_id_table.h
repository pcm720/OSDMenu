#ifndef _GAME_ID_TABLE_H_
#define _GAME_ID_TABLE_H_

// The original is sourced from
// https://github.com/alex-free/tonyhax/blob/master/loader/gameid-psx-exe.c
// and extended further based on https://github.com/niemasd/GameDB-PSX/ and manual verification
const struct {
  const char *volumeTimestamp;
  const char *gameID;
} gameIDTable[] = {
    //
    // SLPS
    //
    {"1994111009000000", "SLPS_000.01"}, // Ridge Racer (Japan) - http://redump.org/disc/2679/
    {"1994110702000000", "SLPS_000.02"}, // Gokujou Parodius Da! Deluxe Pack (Japan) - http://redump.org/disc/5337/
    {"1994102615231700", "SLPS_000.03"}, // Tama: Adventurous Ball in Giddy Labyrinth (Japan) - http://redump.org/disc/6980/
    {"1994110218594700", "SLPS_000.04"}, // A Ressha de Ikou 4: Evolution (Japan) (Rev 0) - http://redump.org/disc/21858/
    {"1995030218052000", "SLPS_000.04"}, // A Ressha de Ikou 4: Evolution (Japan) (Rev 1) - http://redump.org/disc/21858/
    {"1994110722360400", "SLPS_000.05"}, // Mahjong Station Mazin (Japan) (Rev 0) - http://redump.org/disc/63533/
    {"1994120610494900", "SLPS_000.05"}, // Mahjong Station Mazin (Japan) (Rev 1) - http://redump.org/disc/10881/
    {"1994110407000000", "SLPS_000.06"}, // Nekketsu Oyako (Japan) - http://redump.org/disc/10088/
    {"1994111419300000", "SLPS_000.07"}, // Geom Cube (Japan) - http://redump.org/disc/14660/
    {"1994121808190700", "SLPS_000.08"}, // Metal Jacket (Japan) - http://redump.org/disc/5927/
    {"1994121917000000", "SLPS_000.09"}, // Cosmic Race (Japan) - http://redump.org/disc/16058/
    {"1995052918000000", "SLPS_000.10"}, // Falcata: Astran Pardma no Monshou (Japan) - http://redump.org/disc/1682/
    {"1994110220020600", "SLPS_000.11"}, // A Ressha de Ikou 4: Evolution (Japan) (Hatsubai Kinen Gentei Set) - http://redump.org/disc/70160/
    {"1994121518000000", "SLPS_000.13"}, // Raiden Project (Japan) - http://redump.org/disc/3774/
    {"1994103000000000", "SLPS_000.14"}, // Mahjong Gokuu Tenjiku (Japan) - http://redump.org/disc/17392/
    {"1994101813262400", "SLPS_000.15"}, // TwinBee Taisen Puzzle-dama (Japan) - http://redump.org/disc/22905/
    {"1994112617300000", "SLPS_000.16"}, // Jikkyou Powerful Pro Yakyuu '95 (Japan) (Rev 0) - http://redump.org/disc/9552/
    {"1994121517300000", "SLPS_000.16"}, // Jikkyou Powerful Pro Yakyuu '95 (Japan) (Rev 1) - http://redump.org/disc/27931/
    {"1994111013000000", "SLPS_000.17"}, // King's Field (Japan) - http://redump.org/disc/7072/
    {"1994111522183200", "SLPS_000.18"}, // Twin Goddesses (Japan) - http://redump.org/disc/7885/
    {"1994112918000000", "SLPS_000.19"}, // Kakinoki Shougi (Japan) - http://redump.org/disc/22869/
    {"1994111721302100", "SLPS_000.20"}, // Houma Hunter Lime: Special Collection Vol. 1 (Japan) - http://redump.org/disc/18606/
    {"1994100617242100", "SLPS_000.21"}, // Kikuni Masahiko Jirushi: Warau Fukei-san Pachi-Slot Hunter (Japan) - http://redump.org/disc/33816/
    {"1995030215000000", "SLPS_000.22"}, // Starblade Alpha (Japan) - http://redump.org/disc/4664/
    {"1994122718351900", "SLPS_000.23"}, // CyberSled (Japan) - http://redump.org/disc/7879/
    {"1994092920284600", "SLPS_000.24"}, // Myst (Japan) (Rev 0) - http://redump.org/disc/4786/
                                         // Myst (Japan) (Rev 1) - http://redump.org/disc/33887/
                                         // Myst (Japan) (Rev 2) -  http://redump.org/disc/1488/
    {"1994113012000000", "SLPS_000.25"}, // Toushinden (Japan) (Rev 0) - http://redump.org/disc/1560/
    {"1995012512000000", "SLPS_000.25"}, // Toushinden (Japan) (Rev 1) - http://redump.org/disc/23826/
    {"1995041921063500", "SLPS_000.26"}, // Rayman (Japan) - http://redump.org/disc/33719/
    {"1994121500000000", "SLPS_000.27"}, // Kileak, The Blood (Japan) - http://redump.org/disc/14371/
    {"1994121017582300", "SLPS_000.28"}, // Jigsaw World (Japan) - http://redump.org/disc/14455/
    {"1995022623000000", "SLPS_000.29"}, // Idol Janshi Suchie-Pai Limited (Japan) - http://redump.org/disc/33789/
    {"1995050116000000", "SLPS_000.30"}, // Game no Tatsujin (Japan) (Rev 0) -http://redump.org/disc/36035/
    {"1995060613000000", "SLPS_000.30"}, // Game no Tatsujin (Japan) (Rev 1) - http://redump.org/disc/37866/
    {"1995021802000000", "SLPS_000.31"}, // Kyuutenkai (Japan) - http://redump.org/disc/37548/
    {"1995021615022900", "SLPS_000.32"}, // Uchuu Seibutsu Flopon-kun P! (Japan) - http://redump.org/disc/18814/
    {"1995080809000000", "SLPS_000.33"}, // Boxer's Road (Japan) (Rev 0) - http://redump.org/disc/2765/
    {"1995100209000000", "SLPS_000.33"}, // Boxer's Road (Japan) (Rev 1) - http://redump.org/disc/6537/
    {"1995071821394900", "SLPS_000.34"}, // Zeitgeist (Japan) - http://redump.org/disc/16333/
    {"1995042506300000", "SLPS_000.35"}, // Mobile Suit Gundam (Japan) - http://redump.org/disc/3080/
    {"1995011411551700", "SLPS_000.37"}, // Pachio-kun: Pachinko Land Daibouken (Japan) - http://redump.org/disc/36504/
    {"1995041311392800", "SLPS_000.38"}, // Nichibutsu Mahjong: Joshikou Meijinsen (Japan) - http://redump.org/disc/35101/
    {"1995031205000000", "SLPS_000.40"}, // Tekken (Japan) (Rev 0) - http://redump.org/disc/671/
    {"1995061612000000", "SLPS_000.40"}, // Tekken (Japan) (Rev 1) - http://redump.org/disc/1807/
    {"1995040509000000", "SLPS_000.41"}, // Gussun Oyoyo (Japan) - http://redump.org/disc/11336/
    {"1995052612000000", "SLPS_000.43"}, // Mahjong Ganryuu-jima (Japan) - http://redump.org/disc/33772/
    {"1995042500000000", "SLPS_000.44"}, // Hebereke Station Popoitto (Japan) - http://redump.org/disc/36164/
    {"1995033100003000", "SLPS_000.47"}, // Missland (Japan) - http://redump.org/disc/10869/
    {"1995041400000000", "SLPS_000.48"}, // Gokuu Densetsu: Magic Beast Warriors (Japan) - http://redump.org/disc/24258/
    {"1995050413421800", "SLPS_000.50"}, // Night Striker (Japan) - http://redump.org/disc/10931/
    {"1995040509595900", "SLPS_000.51"}, // Entertainment Jansou: That's Pon! (Japan) - http://redump.org/disc/34808/
    {"1995030103150000", "SLPS_000.52"}, // Kanazawa Shougi '95 (Japan) - http://redump.org/disc/34246/
    {"1995100409235300", "SLPS_000.53"}, // Thoroughbred Breeder II Plus (Japan) - http://redump.org/disc/33282/
    {"1995060504013600", "SLPS_000.55"}, // Cyberwar (Japan) (Disc 1) - http://redump.org/disc/30637/
    {"1995060319142200", "SLPS_000.55"}, // Cyberwar (Japan) (Disc 2) - http://redump.org/disc/30638/
    {"1995060402110800", "SLPS_000.55"}, // Cyberwar (Japan) (Disc 3) - http://redump.org/disc/30639/
    {"1995081612000000", "SLPS_000.59"}, // Taikyoku Shougi: Kiwame (Japan) - http://redump.org/disc/35288/
    {"1995051201000000", "SLPS_000.60"}, // Aquanaut no Kyuujitsu (Japan) - http://redump.org/disc/16984/
    {"1995051700000000", "SLPS_000.61"}, // Ace Combat (Japan) - http://redump.org/disc/1691/
    {"1995051002471900", "SLPS_000.63"}, // Keiba Saishou no Housoku '95 (Japan) - http://redump.org/disc/22944/
    {"1995083112000000", "SLPS_000.65"}, // Tokimeki Memorial: Forever with You (Japan) (Rev 1) - http://redump.org/disc/6789/
                                         // Tokimeki Memorial: Forever with You (Japan) (Shokai Genteiban) (Rev 1) - http://redump.org/disc/6788/
    {"1995111700000000", "SLPS_000.65"}, // Tokimeki Memorial: Forever with You (Japan) (Rev 2) - http://redump.org/disc/33338/
    {"1996033100000000", "SLPS_000.65"}, // Tokimeki Memorial: Forever with You (Japan) (Rev 4) - http://redump.org/disc/6764/
    {"1995051816000000", "SLPS_000.66"}, // Kururin Pa! (Japan) - http://redump.org/disc/33413/
    {"1995061418000000", "SLPS_000.67"}, // Jikkyou Powerful Pro Yakyuu '95: Kaimakuban (Japan) - http://redump.org/disc/14367/
    {"1995061911303400", "SLPS_000.68"}, // J.League Jikkyou Winning Eleven (Japan) (Rev 0) - http://redump.org/disc/6740/
    {"1995072800300000", "SLPS_000.68"}, // J.League Jikkyou Winning Eleven (Japan) (Rev 1) - http://redump.org/disc/2848/
    {"1995061207000000", "SLPS_000.69"}, // King's Field II (Japan) - http://redump.org/disc/5892/
    {"1995062922000000", "SLPS_000.70"}, // Street Fighter: Real Battle on Film (Japan) - http://redump.org/disc/26158/
    {"1995040719355400", "SLPS_000.71"}, // 3x3 Eyes: Kyuusei Koushu (Disc 1) (Japan) - http://redump.org/disc/7881/
                                         // 3x3 Eyes: Kyuusei Koushu (Disc 2) (Japan) - http://redump.org/disc/7880/
    {"1995061806364400", "SLPS_000.73"}, // Dragon Ball Z: Ultimate Battle 22 (Japan) - http://redump.org/disc/10992/
    {"1995051015300000", "SLPS_000.77"}, // Douga de Puzzle da! Puppukupuu (Japan) - http://redump.org/disc/11935/
    {"1995070302000000", "SLPS_000.78"}, // Gakkou no Kowai Uwasa: Hanako-san ga Kita!! (Japan) - http://redump.org/disc/11935/
    {"1995070523450000", "SLPS_000.83"}, // Zero Divide (Japan) - http://redump.org/disc/99925/
    {"1995072522004900", "SLPS_000.85"}, // Houma Hunter Lime: Special Collection Vol. 2 (Japan) - http://redump.org/disc/18607/
    {"1995070613170000", "SLPS_000.88"}, // Ground Stroke: Advanced Tennis Game (Japan) - http://redump.org/disc/33778/
    {"1995082517551900", "SLPS_000.89"}, // The Oni Taiji!!: Mezase! Nidaime Momotarou (Japan) - http://redump.org/disc/33948/
    {"1995082109402500", "SLPS_000.90"}, // Eisei Meijin (Japan) (Rev 1) - http://redump.org/disc/37494/
    {"1995053117000000", "SLPS_000.91"}, // Exector (Japan) - http://redump.org/disc/2814/
    {"1995081100000000", "SLPS_000.92"}, // King of Bowling (Japan) - http://redump.org/disc/34727/
    {"1995071011035200", "SLPS_000.93"}, // Oh-chan no Oekaki Logic (Japan) - http://redump.org/disc/7882/
    {"1995090510000000", "SLPS_000.94"}, // Thunder Storm & Road Blaster (Disc 1) (Thunder Storm) (Japan) - http://redump.org/disc/6740/
    {"1995083123000000", "SLPS_000.94"}, // Thunder Storm & Road Blaster (Disc 2) (Road Blaster) (Japan) - http://redump.org/disc/8551/
    {"1995100601300000", "SLPS_000.99"}, // Moero!! Pro Yakyuu '95: Double Header (Japan) - http://redump.org/disc/34818/
    {"1995081001450000", "SLPS_001.01"}, // Universal-ki Kanzen Kaiseki: Pachi-Slot Simulator (Japan) - http://redump.org/disc/36304/
    {"1995080316000000", "SLPS_001.03"}, // V-Tennis (Japan) - http://redump.org/disc/22684/
    {"1995081020000000", "SLPS_001.04"}, // Gouketsuji Ichizoku 2: Chotto dake Saikyou Densetsu (Japan) - http://redump.org/disc/12680/
    {"1995090722000000", "SLPS_001.08"}, // Darkseed (Japan) - http://redump.org/disc/1640/
    {"1995090516062841", "SLPS_001.13"}, // Sotsugyou II: Neo Generation (Japan) - http://redump.org/disc/7885/
    {"1995082016003000", "SLPS_001.28"}, // Makeruna! Makendou 2 (Japan) - http://redump.org/disc/37537/
    {"1995102101350000", "SLPS_001.33"}, // D no Shokutaku: Complete Graphics (Japan) (Disc 1) - http://redump.org/disc/763/
    {"1995102102521200", "SLPS_001.33"}, // D no Shokutaku: Complete Graphics (Japan) (Disc 2) - http://redump.org/disc/764/
    {"1995102105003200", "SLPS_001.33"}, // D no Shokutaku: Complete Graphics (Japan) (Disc 3) - http://redump.org/disc/765/
    {"1995100910002200", "SLPS_001.37"}, // Keiba Saishou no Housoku '96 Vol. 1 (Japan) - http://redump.org/disc/22945/
    {"1995101801325900", "SLPS_001.42"}, // Senryaku Shougi (Japan) - http://redump.org/disc/61170/
    {"1995113010450000", "SLPS_001.46"}, // Keiba Saishou no Housoku '96 Vol. 1 (Japan) - http://redump.org/disc/22945/
    {"1995092205430500", "SLPS_001.52"}, // Yaku: Yuujou Dangi (Japan) - http://redump.org/disc/4668/
    {"1995121620000000", "SLPS_001.73"}, // Alnam no Kiba: Juuzoku Juuni Shinto Densetsu (Japan) - http://redump.org/disc/11199/
    {"1995122811000000", "SLPS_001.90"}, // Welcome House (Japan), missing SYSTEM.CNF - http://redump.org/disc/23332/
    {"1995111622323000", "SLPS_002.01"}, // Magical Drop (Japan), missing SYSTEM.CNF - http://redump.org/disc/24773/
    {"1995121418400300", "SLPS_002.30"}, // CG Mukashi Banashi - Jiisan 2-do Bikkuri!! (Japan), missing SYSTEM.CNF - http://redump.org/disc/18884/
    {"1996010800000000", "SLPS_002.61"}, // Sotsugyou R - Graduation Real (Japan), missing SYSTEM.CNF - http://redump.org/disc/7892/
    {"1996022700000000", "SLPS_003.21"}, // Tetris X (Japan), missing SYSTEM.CNF - http://redump.org/disc/35855/
    {"1996020413401600", "SLPS_003.36"}, // Sid Meier's Civilization - Shin Sekai Shichidai Bunmei (Japan), missing SYSTEM.CNF - http://redump.org/disc/5607/
    {"1996030619500500", "SLPS_003.37"}, // Nobunaga Shippuuki - Kirameki (Japan), missing SYSTEM.CNF - http://redump.org/disc/30963/
    {"1996072211000000", "SLPS_005.49"}, // DigiCro: Digital Number Crossword (Japan), missing SYSTEM.CNF - http://redump.org/disc/6400/
    {"1997011500000000", "SLPS_007.19"}, // The Great Battle VI (Japan) - http://redump.org/disc/37406/
    {"1997031012200700", "SLPS_008.78"}, // FIFA Soccer 97 (Japan) - http://redump.org/disc/34407/
    {"1997050817540700", "SLPS_008.95"}, // Over Drivin' II (Japan) - http://redump.org/disc/2088/
    {"1998061000000000", "SLPS_013.34"}, // Himitsu Kessha Q (Japan), missing SYSTEM.CNF - http://redump.org/disc/60635/
    {"1998040820350000", "SLPS_015.58"}, // The Crown Knights - Jaja-Uma! Quartet - Mega Dream Destruction+ (Japan), missing SYSTEM.CNF - http://redump.org/disc/34399/

    //
    // SCPS
    //
    {"1994112112000000", "SCPS_100.01"}, // Motor Toon Grand Prix (Japan), missing SYSTEM.CNF - http://redump.org/disc/3834/
    {"1995011010000000", "SCPS_100.01"}, // Motor Toon Grand Prix (Japan) (Rev 1), missing SYSTEM.CNF - http://redump.org/disc/3835/
    {"1995030717020700", "SCPS_100.02"}, // Victory Zone (Japan), missing SYSTEM.CNF - http://redump.org/disc/37010/
    {"1994103110000000", "SCPS_100.03"}, // Crime Crackers (Japan), missing SYSTEM.CNF - http://redump.org/disc/5729/
    {"1995022100000000", "SCPS_100.04"}, // Shanghai - Banri no Choujou (Japan) - http://redump.org/disc/1784/
                                         // Shanghai - Banri no Choujou (Japan) (Gentei Box), SCPS_100.05 - http://redump.org/disc/3608/
    {"1995032500000000", "SCPS_100.06"}, // Gunners Heaven (Japan), missing SYSTEM.CNF - http://redump.org/disc/3880/
    {"1995032400000000", "SCPS_100.07"}, // Jumping Flash! Aloha Danshaku Funky Daisakusen no Maki (Japan) - http://redump.org/disc/4051/
    {"1995052420065100", "SCPS_100.08"}, // Arc the Lad (Japan) (Rev 0) - http://redump.org/disc/67966/
                                         // Arc the Lad (Japan) (Rev 1) - http://redump.org/disc/1472/
    {"1995061723590000", "SCPS_100.09"}, // Philosoma (Japan), missing SYSTEM.CNF — http://redump.org/disc/3778/
    {"1995080914422700", "SCPS_100.10"}, // Wizardry VII - Guardia no Houju (Japan), missing SYSTEM.CNF - http://redump.org/disc/1438/
    {"1995071219364500", "SCPS_100.12"}, // Hermie Hopperhead - Scrap Panic (Japan), missing SYSTEM.CNF - http://redump.org/disc/30748/
    {"1995092719000000", "SCPS_100.14"}, // Beyond the Beyond - Haruka naru Kanaan e (Japan) - http://redump.org/disc/602/
    {"1995103122331500", "SCPS_100.16"}, // Horned Owl (Japan), missing SYSTEM.CNF — http://redump.org/disc/4667/
};

#endif
