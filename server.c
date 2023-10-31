 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//outras bibliotecas
#include <unistd.h>
#include <pthread.h>

#define BUFSZ 500


//FUNÇÕES REDES
void logexit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void encerraPrograma(int argc, char **argv)
{
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize)
{
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = "";
    uint16_t port;

    if (addr->sa_family == AF_INET)
    {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET6_ADDRSTRLEN + 1))
        {
            logexit("ntop");
        }
        port = ntohs(addr4->sin_port); // network to host short
    }
    else if (addr->sa_family == AF_INET6)
    {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
        {
            logexit("ntop");
        }
        port = ntohs(addr6->sin6_port); // network to host short
    }
    else
    {
        logexit("unknown protocol family.");
    }
    if (str)
    {
        snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
    }
}

int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage)
{
    uint16_t port = (uint16_t)atoi(portstr);
    if (port == 0)
    {
        return -1;
    }
    port = htons(port);

    memset(storage, 0, sizeof(*storage));

    if(0 == strcmp(proto, "v4"))
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr.s_addr = INADDR_ANY;
        return 0;
    }
    else if(0 == strcmp(proto, "v6"))
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        addr6->sin6_addr = in6addr_any;
        return 0;
    }
    else return -1;
}

//FUNÇÕES SERVIDOR
void saidasPrograma(int socketCliente1, const char *msg)
{
    int conf = send(socketCliente1, msg, strlen(msg) + 1, 0);
    if(conf != (strlen(msg) + 1))
    {
        logexit("send");
    }
}

int le_sensores(int *lidos, char recebido[60], int *equipamento, int funcao)
{
    // funcao define se para de ler no 3 ou no 4
    int posicao, cont = 0;
    if (funcao == 3)
        posicao = 12;
    else
        posicao = 6;
    *lidos = ((int) recebido[posicao]) - 48;
    posicao += 3;
    if (recebido[posicao] == 'n')
    {
        *equipamento = (int)recebido[posicao + 3] - 48;
    }
    else
    {
        cont++;
        *(lidos+1) = (int)recebido[posicao] - 48;
        if (recebido[posicao+3] == 'n')
        {
            *equipamento = (int)recebido[posicao + 6] - 48;
        }
        else
        {
            cont++;
            *(lidos+2) = (int)recebido[posicao + 3] - 48;
            if (funcao = 3)
            {
                *equipamento = (int)recebido[posicao + 9] - 48;
            }
            else
            { //QUANDO FOR CORRIGIR A ULTIMA FUNÇÃO VOU TER Q MEXER NISSO SE PA
                *(lidos+3) = (int)recebido[posicao + 6] - 48;
                *equipamento = (int)recebido[posicao + 9] - 48;
            }
        }
    }
    return cont;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        encerraPrograma(argc, argv);
    }

    //REDES
    struct sockaddr_storage storage;
	if (0 != server_sockaddr_init(argv[1], argv[2], &storage))
    {
		encerraPrograma(argc, argv);
	}
    
    int socketUtilizado = socket(storage.ss_family, SOCK_STREAM, 0);
    if(socketUtilizado == -1)
    {
        logexit("socket");
    }

    int enable = 1;
    if(0 != setsockopt(socketUtilizado, SOL_SOCKET,SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("enable");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(0 != bind(socketUtilizado, addr, sizeof(storage)))
    {
        logexit("bind");
    }
   
    if(0 != listen(socketUtilizado, 10))
    {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    
    while(1)
    {
        struct sockaddr_storage storageClient;
        struct sockaddr *addrClient = (struct sockaddr *)(&storageClient);
        socklen_t addrlenClient = sizeof(storageClient);

        int socketCliente = accept(socketUtilizado, addrClient, &addrlenClient);
        
        if(socketCliente == -1)
        {
            logexit("accept");
        }

        char addrstrCliente[BUFSZ];
        addrtostr(addrClient, addrstrCliente, BUFSZ);
        printf("conectou >> %s\n", addrstrCliente);

        char buf[BUFSZ];

        //VARIÁVEIS USADAS PARA RODAR AS OPERAÇÕES
        RAND_MAX;
        char funcao[6] = "", solicitacao[60], mensagemFinal[50] = "", nmrs[35] = "";
        int limiteSensores = 0, equipamentoId, cont, sensorAtual = 0, totalSensoresNaoInstalados, adicionou, adicionaMesmoAssim = 1;
        int sensorId[3] = {0, 0, 0}, sensoresNaoInstalados[3], sensorExistente[3];
        float leituraSensorf;
        int equipamentos[5][5];

        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                equipamentos[i][j] = 0;
            }
        }

        //RECEBENDO AS MENSAGENS E LIDANDO COM AS OPERAÇÕES
        while(1)
        {
            //LIDANDO COM AS ENTRADAS
            memset(buf, 0, BUFSZ);
            size_t count = recv(socketCliente, buf, BUFSZ - 1, 0);
            printf(">>> msg recebida >> %s || %d || %s\n", addrstrCliente, (int)count, buf);

            memset(solicitacao, 0, 60);
            strcpy(solicitacao, buf);
            
            //OPERAÇÕES
            memset(funcao, 0, 6);

            //CONFERINDO ENTRADAS VÁLIDAS
            int p = 0;
            while(solicitacao[p] != ' ')
            {
                funcao[p] = solicitacao[p];
                p++;
            }

            if(strcmp(funcao, "kill") == 0)
            {
                logexit("entrada inválida");
            }
            
            // ADICIONA UM OU MAIS SENSORES EM UM EQUIPAMENTO
            else if (strcmp(funcao, "add") == 0)
            {
                cont = le_sensores(&sensorId[0], solicitacao, &equipamentoId, 3);
                adicionaMesmoAssim = 1;

                if(equipamentoId > 4)
                {
                    printf("invalid equipament\n");
                    saidasPrograma(socketCliente, "invalid equipament\n");
                }
                else{
                    if (limiteSensores > 15)
                    {
                        printf("limit exceeded\n");
                        saidasPrograma(socketCliente, "limit exceeded\n");
                    }
                    else{
                        int i = 0, j = 0;
                        for (; i <= cont; i++)
                        {
                            sensorAtual = sensorId[i];
                            if(sensorAtual > 4)
                            {
                                printf("invalid sensor\n");
                                saidasPrograma(socketCliente, "invalid sensor\n");
                                adicionou = -1;
                                break;
                            }
                        }
                        if (adicionou >= 0)
                        {
                            printf("sensor");
                            strcpy(mensagemFinal, "sensor");
                            for (i = 0; i <= cont; i++)
                            {
                                sensorAtual = sensorId[i];
                                int sens = 0;
                                while((equipamentos[equipamentoId][sens] != sensorAtual) && (sens < 3))
                                {
                                    if((equipamentos[equipamentoId][sens] == sensorAtual))
                                    {
                                        sensorExistente[j] = sensorAtual;
                                        adicionou = 1;
                                        j++;
                                        sens+=4;
                                    }
                                    sens++;
                                }
                                if (sens < 3)
                                {
                                    adicionou = 1;
                                    printf(" 0%d", sensorAtual);
                                    sprintf(nmrs, " 0%d", sensorAtual);
                                    strcat(mensagemFinal, nmrs);
                                    sensorId[i] = 0;
                                }
                            }
                            if (adicionou == 1)
                            {
                                printf(" already exist in 0%d\n", equipamentoId);
                                sprintf(nmrs, " already exist in 0%d\n", equipamentoId);
                                strcat(mensagemFinal, nmrs);
                                saidasPrograma(socketCliente, mensagemFinal);
                                memset(mensagemFinal, 0, 50);
                                memset(nmrs, 0, 35);
                            }

                            for(j = 0; j <= 3; j++)
                            {
                                if(equipamentos[equipamentoId][j] == 0) break;
                            }
                            for (i = 0; i <= cont; i++)
                            {
                                sensorAtual = sensorId[i];
                                if(sensorAtual != 0)
                                {
                                    equipamentos[equipamentoId][j] = sensorAtual;
                                    printf(" 0%d", sensorAtual);
                                    sprintf(nmrs, " 0%d", equipamentos[equipamentoId][j]);
                                    strcat(mensagemFinal, nmrs);
                                    adicionou = 2;
                                    limiteSensores++;
                                    j++;
                                    equipamentos[equipamentoId][j] = sensorAtual;
                                }
                            }
                            if (adicionou == 2)
                            {
                                printf(" added\n");
                                strcat(mensagemFinal, " added\n");
                                saidasPrograma(socketCliente, mensagemFinal);
                                memset(mensagemFinal, 0, 50);
                                memset(nmrs, 0, 35);
                            }
                        }
                    }
                }
                sensorAtual = 0;
                adicionou = 50;
            }

            // REMOVE UM SENSOR DE UM EQUIPAMENTO
            else if (strcmp(funcao, "remove") == 0)
            {
                equipamentoId = (int) solicitacao[21] - 48;

                if(equipamentoId > 4)
                {
                    printf("invalid equipament\n");
                    saidasPrograma(socketCliente, "invalid equipament\n");
                }
                else
                {
                    sensorAtual = (int)solicitacao[15] - 48;
                    if(sensorAtual > 4)
                    {
                        printf("invalid sensor\n");
                        saidasPrograma(socketCliente, "invalid sensor\n");
                        adicionaMesmoAssim = 0;
                    }
                    else
                    {
                        for (int i = 0; i < 4; i++){
                            if(equipamentos[equipamentoId][i] == sensorAtual){
                                for (int j = i+1; j < 4; j++){
                                    equipamentos[equipamentoId][i] = equipamentos[equipamentoId][j];
                                    i++;
                                }
                                if(adicionaMesmoAssim != 0)
                                {
                                    printf("sensor 0%d removed\n", sensorAtual);
                                    sprintf(nmrs, "sensor 0%d removed\n", sensorAtual);
                                    strcat(mensagemFinal, nmrs);
                                    saidasPrograma(socketCliente, mensagemFinal);

                                }   
                                memset(mensagemFinal, 0, 50);
                                memset(nmrs, 0, 35);
                                break;
                            }
                            if(i == 3 && adicionaMesmoAssim != 0)
                            {
                                printf("sensor 0%d does not exist in 0%d\n", sensorAtual, equipamentoId);
                                sprintf(nmrs, "sensor 0%d does not exist in 0%d\n", sensorAtual, equipamentoId);
                                strcat(mensagemFinal, nmrs);
                                saidasPrograma(socketCliente, mensagemFinal);
                                memset(mensagemFinal, 0, 50);
                                memset(nmrs, 0, 35);
                            }
                        }
                    }
                    sensorAtual = 0;
                }
            }

            // RETORNA UMA LISTA COM OS SENSORES ADICIONADOS
            else if (strcmp(funcao, "list") == 0)
            {
                equipamentoId = (int) solicitacao[17] - 48;

                if(equipamentoId > 4)
                {
                    printf("invalid equipament\n");
                    saidasPrograma(socketCliente, "invalid equipament\n");
                }
                else
                {
                    int sens = 0;
                    while((equipamentos[equipamentoId][sens] != 0) && (sens < 4)){
                        printf("0%d ", equipamentos[equipamentoId][sens]);
                        sprintf(nmrs, "0%d ", equipamentos[equipamentoId][sens]);
                        strcat(mensagemFinal, nmrs);
                        sens++;
                    }
                    if (sens == 0) 
                    {
                        printf("none");
                        saidasPrograma(socketCliente, "none\n");
                    }
                    else
                    {
                        strcat(mensagemFinal, "\n");
                        saidasPrograma(socketCliente, mensagemFinal);
                        memset(mensagemFinal, 0, 50);
                        memset(nmrs, 0, 35);
                    }
                    printf("\n");
                }
            }

            // RETORNA AS "LEITURAS" DE CADA SENSOR
            else if (strcmp(funcao, "read") == 0)
            {
                for (int i = 0; i <= cont; i++)
                    {
                        for(int j = 0; j < 4; j++)
                        {
                            if(equipamentos[equipamentoId][i] != 0 && sensorId[j] != 0)
                            {
                                if (equipamentos[equipamentoId][i] == sensorId[j])
                                {
                                    i++;
                                    j = -1;//0;
                                }
                                else if(equipamentos[equipamentoId][i] != sensorId[j] && j == 3)
                                {
                                    sensoresNaoInstalados[totalSensoresNaoInstalados] = sensorId[j];
                                    totalSensoresNaoInstalados++;
                                }
                            }
                        }
                        if (totalSensoresNaoInstalados > 0)
                        {
                            printf("sensor(s)");
                            strcat(mensagemFinal, "sensor(s)");
                            for (int i = 0; i < totalSensoresNaoInstalados; i++)
                            {
                                printf(" 0%d ", sensoresNaoInstalados[i]);
                                sprintf(nmrs, " 0%d ", sensoresNaoInstalados[i]);
                                strcat(mensagemFinal, nmrs);
                            }
                            printf("not installed\n");
                            strcat(mensagemFinal, " not installed\n");
                            saidasPrograma(socketCliente, mensagemFinal);
                            memset(mensagemFinal, 0, 50);
                            memset(nmrs, 0, 35);
                        }
                        else
                        {
                            for (int i = 0; i < totalSensoresNaoInstalados; i++)
                            {
                                leituraSensorf = ((float) (rand()%999)) / 100.0;
                                printf("%.2f\n", leituraSensorf);
                                sprintf(nmrs, "%.2f\n", leituraSensorf);
                                strcat(mensagemFinal, nmrs);
                                totalSensoresNaoInstalados++;
                            }
                            saidasPrograma(socketCliente, mensagemFinal);
                            memset(mensagemFinal, 0, 50);
                            memset(nmrs, 0, 35);
                        }
                    }
                }
                cont = 0;
            }
            else
            {
                exit(EXIT_FAILURE);
            }

            memset(mensagemFinal, 0, 50);
            memset(nmrs, 0, 35);
        }
        
    }

    return 0;
}
